#pragma once

#include "pmm.hpp"
#include "vmm.hpp"
#include <cstdint>
#include <cstddef>

struct HeapBlock {
    size_t size;
    bool free;
    HeapBlock* next;
    HeapBlock* prev;
    uint32_t magic;
    
    static constexpr uint32_t defaultMagic = 0x1248ace0;
    
    void* getData() {
        return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(this) + sizeof(HeapBlock));
    }
    
    static HeapBlock* fromData(void* data) {
        return reinterpret_cast<HeapBlock*>(reinterpret_cast<uint64_t>(data) - sizeof(HeapBlock));
    }
    
    bool isValid() const {
        return magic == defaultMagic;
    }
};

class Heap {
public:
    Heap() : startLocation(nullptr), endLocation(nullptr), firstBlock(nullptr), 
             initialized(false), totalSize(0), usedSize(0) {}
    

    void init(void* start, size_t size);

    void* allocate(size_t size);
    void* allocateAligned(size_t size, size_t alignment);
    void free(void* ptr);
    void* reallocate(void* ptr, size_t newSize);


    size_t getTotalSize() const { return totalSize; }
    size_t getUsedSize() const { return usedSize; }
    size_t getFreeSize() const { return totalSize - usedSize; }
    
    bool isInitialized() const { return initialized; }
    
    bool expand(size_t finalSize);
    
private:
    void* startLocation;
    void* endLocation;
    HeapBlock* firstBlock;
    bool initialized;
    size_t totalSize;
    size_t usedSize;

    HeapBlock* findFreeBlock(size_t size);
    void splitBlock(HeapBlock* block, size_t size);
    void mergeBlocks(HeapBlock* block);
    
    static size_t alignSize(size_t size) {
        return (size + 15) & ~15;
    }
};

extern Heap kheap;