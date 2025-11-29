#include "syscall.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/process/exec.hpp>
#include <fs/vfs/vfs.hpp>
#include <graphics/console.hpp>
#include <interrupts/keyboard.hpp>
#include <interrupts/timer.hpp>
#include <x86_64/requests.hpp>
#include <x86_64/ports.hpp>
#include <string.h>

extern Console* console;
extern Keyboard* globalKeyboard;
extern Timer* globalTimer;
extern "C" uint64_t kernelStackTop;

Syscall syscallInstance;

bool hasSyscall(){
    uint32_t eax = 0x80000001, ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);
    return (edx >> 11) & 1;
}

Syscall& Syscall::get() {
    return syscallInstance;
}

void Syscall::initialize() {
    if (initialized) return;
    initialized = true;

    if(!hasSyscall()){
        console->drawText("No 'syscall' instruction support. halting...");
        asm volatile("cli");
        while(1);
    }

    
}

void Syscall::setKernelStack(uint64_t stack) {
    kernelStackTop = stack;
}

uint64_t Syscall::handle(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch ((SyscallNumber)syscall_num) {
        using enum SyscallNumber;
        case Exit:
            return sys_exit(arg1);
        case Write:
            return sys_write(arg1, arg2, arg3);
        case Read:
            return sys_read(arg1, arg2, arg3);
        case Open:
            return sys_open(arg1, arg2, arg3);
        case Close:
            return sys_close(arg1);
        case GetPID:
            return sys_getpid();
        case Fork:
            return sys_fork();
        case Exec:
            return sys_exec(arg1, arg2, arg3);
        case Wait:
            return sys_wait(arg1, arg2);
        case Kill:
            return sys_kill(arg1, arg2);
        case Mmap:
            return sys_mmap(arg1, arg2, arg3);
        case Munmap:
            return sys_munmap(arg1, arg2);
        case Yield:
            return sys_yield();
        case Sleep:
            return sys_sleep(arg1);
        case GetTime:
            return sys_gettime();
        case Clear:
            return sys_clear();
        case FBInfo:
            return sys_fb_info(arg1);
        case FBMap:
            return sys_fb_map();
        case Signal:
            return sys_signal(arg1, arg2);
        case SigReturn:
            return sys_sigreturn();
        default:
            return (uint64_t)-1;
    }
}

uint64_t Syscall::sys_exit(uint64_t code) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) {
        return (uint64_t)-1;
    }

    current->setExitCode((int)code);
    current->setState(ProcessState::Terminated);
    
    return 0;
}

static bool isValidUserPointer(uint64_t ptr, size_t size) {
    // User space: < 0x0000800000000000
    // Kernel space: >= 0xFFFF800000000000
    if (ptr >= 0xFFFF800000000000) {
        return false;
    }
    if (ptr == 0) {
        return false;
    }

    if (ptr + size < ptr) {
        return false;
    }
    if (ptr + size >= 0x0000800000000000) {
        return false;
    }
    return true;
}

uint64_t Syscall::sys_write(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        if (!console || count == 0) {
            return count;
        }
        
        if (!isValidUserPointer(buf, count)) return -1;
        
        const char* str = reinterpret_cast<const char*>(buf);
        
        for (size_t i = 0; i < count; i++) {
            char temp[2] = { str[i], '\0' };
            console->drawText(temp);
        }
        
        return count;
    }
    
    return -1;
}

uint64_t Syscall::sys_read(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd == 0) {        
        if (!globalKeyboard) {
            if (console) console->drawText("No keyboard!\nPress F2 to continue...\n");
            return -1;
        }
        
        if (!isValidUserPointer(buf, count)) {
            return -1;
        }
        
        for (int i = 0; i < 100; i++) {
            if (globalKeyboard->poll() == 0) break;
        }
        
        char* buffer = reinterpret_cast<char*>(buf);
        size_t bytesRead = 0;
        
        asm volatile("sti");
        
        while (bytesRead < count) {
            char c = globalKeyboard->poll();
            
            if (c == 0) {
                asm volatile("pause");
                continue;
            }
            
            if (c == '\b') {
                if (bytesRead > 0) {
                    bytesRead--;
                    if (console) {
                        console->drawText("\b");
                        console->drawText(" ");
                        console->drawText("\b");
                    }
                }
                continue;
            }
            
            buffer[bytesRead++] = c;
            
            if (console && (c == '\n' || (c >= 32 && c < 127))) {
                char text[2] = {c, '\0'};
                console->drawText(text);
            }
            
            if (c == '\n') {
                break;
            }
        }
        
        asm volatile("cli");
        
        return bytesRead;
    }
    
    return -1;
}

uint64_t Syscall::sys_open(uint64_t path, uint64_t flags, uint64_t mode) {
    return -1;
}

uint64_t Syscall::sys_close(uint64_t fd) {
    return -1;
}

uint64_t Syscall::sys_getpid() {
    Process* current = Scheduler::get().getCurrentProcess();
    return current ? current->getPID() : 0;
}

uint64_t Syscall::sys_fork() {
    return -1;
}

uint64_t Syscall::sys_exec(uint64_t path, uint64_t argv, uint64_t envp __attribute__((unused))) {
    if (!isValidUserPointer(path, 1)) {
        return -1;
    }
    
    const char* userPathname = reinterpret_cast<const char*>(path);
    size_t pathLen = 0;
    while (userPathname[pathLen] && pathLen < 256) pathLen++;
    
    char* pathname = new char[pathLen + 1];
    memcpy(pathname, userPathname, pathLen);
    pathname[pathLen] = '\0';
    
    if (argv != 0 && !isValidUserPointer(argv, sizeof(char*))) {
        delete[] pathname;
        return -1;
    }
    
    const char** userArgv = reinterpret_cast<const char**>(argv);
    int argc = 0;
    
    if (userArgv) {
        while (userArgv[argc] != nullptr && argc < 64) {
            argc++;
        }
    }
    
    const char** kernelArgv = new const char*[argc + 1];
    for (int i = 0; i < argc; i++) {
        const char* userArg = userArgv[i];
        size_t argLen = 0;
        while (userArg[argLen] && argLen < 256) argLen++;
        
        char* kernelArg = new char[argLen + 1];
        memcpy(kernelArg, userArg, argLen);
        kernelArg[argLen] = '\0';
        kernelArgv[i] = kernelArg;
    }
    kernelArgv[argc] = nullptr;
    
    uint64_t userCR3;
    asm volatile("mov %%cr3, %0" : "=r"(userCR3));
    
    extern VMM vmm;
    vmm.load();
    
    Process* newProc = ProcessExecutor::loadUserBinaryWithArgs(pathname, argc, kernelArgv);
    
    asm volatile("mov %0, %%cr3" :: "r"(userCR3) : "memory");
    
    for (int i = 0; i < argc; i++) {
        delete[] kernelArgv[i];
    }
    delete[] kernelArgv;
    delete[] pathname;
    
    if (!newProc) {
        return -1;
    }
    
    Process* current = Scheduler::get().getCurrentProcess();
    if (current) {
        newProc->setParentPID(current->getPID());
    }
    
    Scheduler::get().addProcess(newProc);
    Scheduler::get().scheduleFromSyscall();
    
    return 0;
}

uint64_t Syscall::sys_wait(uint64_t pid, uint64_t statusPtr) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) {
        return (uint64_t)-1;
    }
        
    Process* child = Scheduler::get().getProcessByPID((uint32_t)pid);
    if (!child) {
        return (uint64_t)-1;
    }
    
    if (child->getParentPID() != current->getPID()) {
        return (uint64_t)-1;
    }
    
    if (statusPtr) {
        int* status = reinterpret_cast<int*>(statusPtr);
        *status = 0;
    }
    
    return 0;
}

uint64_t Syscall::sys_kill(uint64_t pid, uint64_t sig) {
    Process* target = Scheduler::get().getProcessByPID((uint32_t)pid);
    if (!target) return -1;
    
    target->sendSignal((int)sig);
    return 0;
}

uint64_t Syscall::sys_mmap(uint64_t addr, uint64_t length, uint64_t prot) {
    return -1;
}

uint64_t Syscall::sys_munmap(uint64_t addr, uint64_t length) {
    return -1;
}

uint64_t Syscall::sys_yield() {
    Scheduler::get().yield();
    return 0;
}

uint64_t Syscall::sys_sleep(uint64_t ms) {
    if (!globalTimer) return -1;
    
    uint64_t start = globalTimer->getMilliseconds();
    uint64_t target = start + ms;
    
    while (globalTimer->getMilliseconds() < target) {
        asm volatile("pause");
    }
    
    return 0;
}

uint64_t Syscall::sys_gettime() {
    if (!globalTimer) {
        return 0;
    }
    return globalTimer->getMilliseconds();
}

uint64_t Syscall::sys_clear() {
    if (console) {
        console->drawText("\033[2J");
    }
    return 0;
}

struct FBInfo {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint16_t bpp;
};

uint64_t Syscall::sys_fb_info(uint64_t info_ptr) {
    if (!console) return (uint64_t)-1;
    
    extern Framebuffer* fb;
    if (!fb) return (uint64_t)-1;
    
    uint64_t fb_kernel_virt = reinterpret_cast<uint64_t>(fb->getRaw());
    uint64_t fb_phys = fb_kernel_virt - hhdm_request.response->offset;
    
    constexpr uint64_t USER_FB_BASE = 0x0000700000000000;
    
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    size_t fb_size = fb->getPitch() * fb->getHeight();
    size_t pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    current->getVMM()->mapRange(
        reinterpret_cast<void*>(USER_FB_BASE),
        reinterpret_cast<void*>(fb_phys),
        pages,
        PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_CACHE_DISABLE
    );
    
    FBInfo* info = reinterpret_cast<FBInfo*>(info_ptr);
    info->addr = USER_FB_BASE;
    info->width = fb->getWidth();
    info->height = fb->getHeight();
    info->pitch = fb->getPitch();
    info->bpp = sizeof(uint32_t);
    
    return 0;
}

uint64_t Syscall::sys_fb_map() {
    if (!console) return (uint64_t)-1;
    
    extern Framebuffer* fb;
    if (!fb) return (uint64_t)-1;
    
    // todo: do actual mapping
    return reinterpret_cast<uint64_t>(fb->getRaw());
}

extern "C" void saveSyscallState(uint64_t* stack) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return;
    
    uint64_t* regs = stack;
    uint64_t* cpuFrame = stack + 15;
    
    uint64_t user_rip = cpuFrame[0];
    uint64_t user_cs = cpuFrame[1];
    uint64_t user_rflags = cpuFrame[2];
    uint64_t user_rsp = cpuFrame[3];
    
    if (user_cs == 0x1B) {
        current->getContext()->rip = user_rip;
        current->getContext()->rsp = user_rsp;
        current->getContext()->rflags = user_rflags;
        
        current->getContext()->rax = regs[0];
        current->getContext()->rbx = regs[1];
        current->getContext()->rcx = regs[2];
        current->getContext()->rdx = regs[3];
        current->getContext()->rsi = regs[4];
        current->getContext()->rdi = regs[5];
        current->getContext()->rbp = regs[6];
        current->getContext()->r8 = regs[7];
        current->getContext()->r9 = regs[8];
        current->getContext()->r10 = regs[9];
        current->getContext()->r11 = regs[10];
        current->getContext()->r12 = regs[11];
        current->getContext()->r13 = regs[12];
        current->getContext()->r14 = regs[13];
        current->getContext()->r15 = regs[14];
        
        current->setValidUserState(true);
    }
}

extern "C" uint64_t syscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return Syscall::get().handle(syscall_num, arg1, arg2, arg3, arg4, arg5);
}

uint64_t Syscall::sys_signal(uint64_t sig, uint64_t handler) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    if (sig >= NSIG) return (uint64_t)-1;
    
    SignalHandler* sh = current->getSignalHandler();
    uint64_t old = reinterpret_cast<uint64_t>(sh->handlers[sig]);
    sh->handlers[sig] = reinterpret_cast<sighandler_t>(handler);
    
    return old;
}

uint64_t Syscall::sys_sigreturn() {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    uint64_t* stack = reinterpret_cast<uint64_t*>(current->getContext()->rsp);
    current->getContext()->rip = stack[0];
    current->getContext()->rsp += 128;
    
    return 0;
}
