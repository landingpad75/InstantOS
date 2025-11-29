#include "elf.hpp"
#include <cpu/process/process.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/mm/pmm.hpp>
#include <x86_64/requests.hpp>
#include <string.h>
#include <fs/vfs/vfs.hpp>
#include <graphics/console.hpp>

extern Console* console;
extern "C" void processTrampoline();

bool ELFLoader::validateHeader(const Elf64_Ehdr* ehdr) {
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }
    
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }
    
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }
    
    if (ehdr->e_type != ET_EXEC) {
        return false;
    }
    
    if (ehdr->e_machine != EM_X86_64) {
        return false;
    }
    
    return true;
}

bool ELFLoader::isValidELF(const void* data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) {
        return false;
    }
    
    const Elf64_Ehdr* ehdr = static_cast<const Elf64_Ehdr*>(data);
    return validateHeader(ehdr);
}

Process* ELFLoader::loadELF(const void* data, size_t size) {
    if (!isValidELF(data, size)) {
        return nullptr;
    }
    
    const Elf64_Ehdr* ehdr = static_cast<const Elf64_Ehdr*>(data);
    
    uint32_t pid = Scheduler::get().allocatePID();
    Process* proc = new Process(pid);
    
    const uint8_t* fileData = static_cast<const uint8_t*>(data);
    const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(fileData + ehdr->e_phoff);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;

            uint64_t pageAlignedAddr = vaddr & ~0xFFFULL;
            uint64_t endAddr = vaddr + memsz;
            uint64_t pageAlignedEnd = (endAddr + 0xFFF) & ~0xFFFULL;
            size_t pages = (pageAlignedEnd - pageAlignedAddr) / PAGE_SIZE;
            
            void* physPages = pmm.allocatePages(pages);
            if (!physPages) {
                if (console) {
                    console->drawText("[ELF] Failed to allocate pages\n");
                }
                delete proc;
                return nullptr;
            }
            
            uint64_t virtPages = reinterpret_cast<uint64_t>(physPages) + hhdm_request.response->offset;
            memset(reinterpret_cast<void*>(virtPages), 0, pages * PAGE_SIZE);
            
            if (filesz > 0) {
                uint64_t copyOffset = vaddr - pageAlignedAddr;
                memcpy(reinterpret_cast<void*>(virtPages + copyOffset), 
                       fileData + offset, filesz);
            }
            
            uint64_t flags = PTE_PRESENT | PTE_USER;
            if (phdr[i].p_flags & PF_W) {
                flags |= PTE_WRITABLE;
            }
            
            proc->getVMM()->mapRange(reinterpret_cast<void*>(pageAlignedAddr), 
                                     physPages, pages, flags);
        }
    }
    
    uint64_t userStack = proc->getUserStack();
    userStack &= ~0xFULL;
    
    uint64_t entry = ehdr->e_entry;
    uint64_t trampolineAddr = reinterpret_cast<uint64_t>(&processTrampoline);
    
    uint64_t kernelStack = proc->getKernelStack();
    kernelStack -= 8;
    *reinterpret_cast<uint64_t*>(kernelStack) = userStack;
    kernelStack -= 8;
    *reinterpret_cast<uint64_t*>(kernelStack) = entry;
    
    proc->getContext()->rip = trampolineAddr;
    proc->getContext()->rsp = kernelStack;
    proc->getContext()->rbp = 0;
    proc->getContext()->rflags = 0x202;

    return proc;
}

void ELFLoader::setupArguments(Process* proc, int argc, const char** argv) {
    if (!proc || argc < 0) return;
    
    uint64_t userStack = proc->getUserStack();
    userStack &= ~0xFULL;
    
    size_t totalStringSize = 0;
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            totalStringSize += strlen(argv[i]) + 1;
        }
    }
    totalStringSize = (totalStringSize + 7) & ~7;
    
    size_t totalSize = totalStringSize + (argc + 1) * sizeof(uint64_t) + sizeof(uint64_t);
    
    userStack -= totalSize;
    userStack &= ~0xFULL;
    
    uint8_t* buffer = new uint8_t[totalSize];
    if (!buffer) return;
    memset(buffer, 0, totalSize);
    
    uint64_t stringBase = userStack + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t);
    size_t stringOffset = 0;
    
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            memcpy(buffer + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t) + stringOffset, argv[i], len);
            
            uint64_t* argvPtr = reinterpret_cast<uint64_t*>(buffer + sizeof(uint64_t) + i * sizeof(uint64_t));
            *argvPtr = stringBase + stringOffset;
            stringOffset += len;
        }
    }
    
    uint64_t* nullPtr = reinterpret_cast<uint64_t*>(buffer + sizeof(uint64_t) + argc * sizeof(uint64_t));
    *nullPtr = 0;
    
    uint64_t* argcPtr = reinterpret_cast<uint64_t*>(buffer);
    *argcPtr = argc;
    
    uint64_t savedCR3;
    asm volatile("mov %%cr3, %0" : "=r"(savedCR3));
    
    proc->getVMM()->load();
    
    memcpy(reinterpret_cast<void*>(userStack), buffer, totalSize);
    
    asm volatile("mov %0, %%cr3" :: "r"(savedCR3) : "memory");
    
    delete[] buffer;
    
    proc->setUserStack(userStack);
    
    uint64_t* userRspOnStack = reinterpret_cast<uint64_t*>(proc->getContext()->rsp + 8);
    *userRspOnStack = userStack;
}

Process* ELFLoader::loadELFWithArgs(const void* data, size_t size, int argc, const char** argv) {
    Process* proc = loadELF(data, size);
    
    if (proc) {
        setupArguments(proc, argc, argv);
    }
    
    return proc;
}

Process* ELFLoader::loadELFFromFile(const char* path) {
    FileDescriptor* fd = nullptr;
    int result = VFS::get().open(path, 0, &fd);
    
    if (result != 0 || !fd) {
        return nullptr;
    }
    
    FileStats stats;
    if (fd->getNode()->ops->stat(fd->getNode(), &stats) != 0) {
        VFS::get().close(fd);
        return nullptr;
    }
    
    size_t size = stats.size;
    
    void* buffer = new uint8_t[size];
    if (VFS::get().read(fd, buffer, size) != static_cast<int64_t>(size)) {
        delete[] static_cast<uint8_t*>(buffer);
        VFS::get().close(fd);
        return nullptr;
    }
    
    VFS::get().close(fd);
    
    Process* proc = loadELF(buffer, size);
    delete[] static_cast<uint8_t*>(buffer);
    
    return proc;
}

Process* ELFLoader::loadELFFromFileWithArgs(const char* path, int argc, const char** argv) {
    FileDescriptor* fd = nullptr;
    int result = VFS::get().open(path, 0, &fd);
    
    if (result != 0 || !fd) {
        return nullptr;
    }
    
    FileStats stats;
    if (fd->getNode()->ops->stat(fd->getNode(), &stats) != 0) {
        VFS::get().close(fd);
        return nullptr;
    }
    
    size_t size = stats.size;
    
    void* buffer = new uint8_t[size];
    if (VFS::get().read(fd, buffer, size) != static_cast<int64_t>(size)) {
        delete[] static_cast<uint8_t*>(buffer);
        VFS::get().close(fd);
        return nullptr;
    }
    
    VFS::get().close(fd);
    
    Process* proc = loadELFWithArgs(buffer, size, argc, argv);
    delete[] static_cast<uint8_t*>(buffer);
    
    return proc;
}
