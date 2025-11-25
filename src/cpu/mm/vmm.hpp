#pragma once

#include "pmm.hpp"
#include "page.hpp"
#include <cstdint>
#include <cstddef>

constexpr uint64_t PTE_PRESENT = (1ULL << 0);
constexpr uint64_t PTE_WRITABLE = (1ULL << 1);
constexpr uint64_t PTE_USER = (1ULL << 2);
constexpr uint64_t PTE_WRITE_THROUGH = (1ULL << 3);
constexpr uint64_t PTE_CACHE_DISABLE = (1ULL << 4);
constexpr uint64_t PTE_ACCESSED = (1ULL << 5);
constexpr uint64_t PTE_DIRTY = (1ULL << 6);
constexpr uint64_t PTE_HUGE = (1ULL << 7);
constexpr uint64_t PTE_GLOBAL = (1ULL << 8);
constexpr uint64_t PTE_NO_EXECUTE = (1ULL << 63);

class VMM {
public:
    VMM();
    void init(PageTable* pml4 = nullptr);

    bool map(void* virt, void* phys, uint64_t flags = PTE_PRESENT | PTE_WRITABLE);
    bool mapRange(void* virt, void* phys, size_t count, uint64_t flags = PTE_PRESENT | PTE_WRITABLE);
    
    bool unmap(void* virt);    
    bool unmapRange(void* virt, size_t count);
    
    void* getPhysical(void* virt);
    
    void load();
    
    static PageTable* getCurrentPageTable();
    
    void cloneKernelMappings();
    
    PageTable* getPageTable() const { return _pml4; }
    bool isInitialized() const { return initialized; }
    
private:
    PageTable* _pml4;
    bool initialized;
    
    PageTable* getOrCreateTable(PageTableEntry& entry);    
    PageTable* getTable(PageTableEntry& entry);
    
    static size_t getPML4Index(void* virt) { return (reinterpret_cast<uint64_t>(virt) >> 39) & 0x1FF; }
    static size_t getPDPTIndex(void* virt) { return (reinterpret_cast<uint64_t>(virt) >> 30) & 0x1FF; }
    static size_t getPDIndex(void* virt) { return (reinterpret_cast<uint64_t>(virt) >> 21) & 0x1FF; }
    static size_t getPTIndex(void* virt) { return (reinterpret_cast<uint64_t>(virt) >> 12) & 0x1FF; }
};

extern VMM vmm;