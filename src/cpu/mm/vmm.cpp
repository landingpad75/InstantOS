
#include "vmm.hpp"
#include <x86_64/requests.hpp>

VMM vmm;

VMM::VMM() : _pml4(nullptr), initialized(false) {}

void VMM::init(PageTable* pml4) {
    if (pml4) {
        _pml4 = (PageTable*)((uint64_t)pml4 + hhdm_request.response->offset);
    } else {
        _pml4 = (PageTable*)((uint64_t)pmm.allocatePage() + hhdm_request.response->offset);
        if (!_pml4) return;

        for (int i = 0; i < 512; i++) {
            _pml4->entries[i].clear();
        }
    }

    initialized = true;
}

PageTable* VMM::getOrCreateTable(PageTableEntry& entry) {
    if (entry.hasFlag(PTE_PRESENT)) {
        uint64_t phys = entry.getAddress();
        uint64_t virt = phys + hhdm_request.response->offset;
        return reinterpret_cast<PageTable*>(virt);

    }
    
    void* page = pmm.allocatePage();
    if (!page) return nullptr;
    
    uint64_t phys = (uint64_t)page;
    PageTable* table = (PageTable*)(phys + hhdm_request.response->offset);
    
    for (int i = 0; i < 512; i++) {
        table->entries[i].clear();
    }
        
    entry.setAddress(reinterpret_cast<uint64_t>(page));
    entry.addFlags(PTE_PRESENT | PTE_WRITABLE);
    
    return table;
}

PageTable* VMM::getTable(PageTableEntry& entry) {
    if (!entry.hasFlag(PTE_PRESENT)) {
        return nullptr;
    }
    return reinterpret_cast<PageTable*>(entry.getAddress());
}

bool VMM::map(void* virt, void* phys, uint64_t flags) {
    if (!initialized) return false;

    size_t pml4Index = getPML4Index(virt);
    size_t pdptIndex = getPDPTIndex(virt);
    size_t pdIndex = getPDIndex(virt);
    size_t ptIndex = getPTIndex(virt);

    PageTable* pdpt = getOrCreateTable(_pml4->entries[pml4Index]);
    if (!pdpt) return false;
    
    if (flags & PTE_USER) {
        _pml4->entries[pml4Index].addFlags(PTE_USER);
    }
        
    PageTable* pd = getOrCreateTable(pdpt->entries[pdptIndex]);
    if (!pd) return false;
    
    if (flags & PTE_USER) {
        pdpt->entries[pdptIndex].addFlags(PTE_USER);
    }
    
    PageTable* pt = getOrCreateTable(pd->entries[pdIndex]);
    if (!pt) return false;
    
    if (flags & PTE_USER) {
        pd->entries[pdIndex].addFlags(PTE_USER);
    }
    
    pt->entries[ptIndex].setAddress(reinterpret_cast<uint64_t>(phys));
    pt->entries[ptIndex].setFlags(flags);

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");

    return true;
}

bool VMM::mapRange(void* virt, void* phys, size_t count, uint64_t flags) {
    uint64_t _virtual = reinterpret_cast<uint64_t>(virt);
    uint64_t physical = reinterpret_cast<uint64_t>(phys);

    for (size_t i = 0; i < count; i++) {
        void* v = reinterpret_cast<void*>(_virtual + i * PAGE_SIZE);
        void* p = reinterpret_cast<void*>(physical + i * PAGE_SIZE);

        if (!map(v, p, flags)) {
            return false;
        }
    }

    return true;
}

bool VMM::unmap(void* virt) {
    if (!initialized) return false;

    size_t pml4Index = getPML4Index(virt);
    size_t pdptIndex = getPDPTIndex(virt);
    size_t pdIndex = getPDIndex(virt);
    size_t ptIndex = getPTIndex(virt);

    PageTable* pdpt = getTable(_pml4->entries[pml4Index]);
    if (!pdpt) return false;

    PageTable* pd = getTable(pdpt->entries[pdptIndex]);
    if (!pd) return false;

    PageTable* pt = getTable(pd->entries[pdIndex]);
    if (!pt) return false;

    pt->entries[ptIndex].clear();

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");

    return true;
}

bool VMM::unmapRange(void* virt, size_t count) {
    uint64_t _virtual = reinterpret_cast<uint64_t>(virt);

    for (size_t i = 0; i < count; i++) {
        void* v = reinterpret_cast<void*>(_virtual + i * PAGE_SIZE);
        if (!unmap(v)) {
            return false;
        }
    }

    return true;
}

void* VMM::getPhysical(void* virt) {
    if (!initialized) return nullptr;

    size_t pml4Index = getPML4Index(virt);
    size_t pdptIndex = getPDPTIndex(virt);
    size_t pdIndex = getPDIndex(virt);
    size_t ptIndex = getPTIndex(virt);

    PageTable* pdpt = getTable(_pml4->entries[pml4Index]);
    if (!pdpt) return nullptr;

    PageTable* pd = getTable(pdpt->entries[pdptIndex]);
    if (!pd) return nullptr;

    PageTable* pt = getTable(pd->entries[pdIndex]);
    if (!pt) return nullptr;

    if (!pt->entries[ptIndex].hasFlag(PTE_PRESENT)) {
        return nullptr;
    }

    uint64_t phys = pt->entries[ptIndex].getAddress();
    uint64_t offset = reinterpret_cast<uint64_t>(virt) & 0xFFF;

    return reinterpret_cast<void*>(phys + offset);
}

void VMM::load() {
    if (!initialized) return;

    asm volatile("mov %0, %%cr3" :: "r"(_pml4) : "memory");
}

PageTable* VMM::getCurrentPageTable() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return reinterpret_cast<PageTable*>(cr3 & ~0xFFF);
}

void VMM::cloneKernelMappings() {
    if (!initialized) return;
    
    PageTable* kernelPML4 = getCurrentPageTable();
    if (!kernelPML4) return;
    
    uint64_t kernelPML4Phys = (uint64_t)kernelPML4 - hhdm_request.response->offset;
    PageTable* kernelPML4Virt = (PageTable*)((uint64_t)kernelPML4Phys + hhdm_request.response->offset);
    
    for (int i = 256; i < 512; i++) {
        _pml4->entries[i] = kernelPML4Virt->entries[i];
    }
}
