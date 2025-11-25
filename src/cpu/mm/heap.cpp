#include "heap.hpp"

Heap kheap;

void Heap::init(void* start, size_t size) {
    startLocation = start;
    endLocation = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(start) + size);
    totalSize = size;
    usedSize = sizeof(HeapBlock);
    
    firstBlock = reinterpret_cast<HeapBlock*>(start);
    firstBlock->size = size - sizeof(HeapBlock);
    firstBlock->free = true;
    firstBlock->next = nullptr;
    firstBlock->prev = nullptr;
    firstBlock->magic = HeapBlock::defaultMagic;
    
    initialized = true;
}

HeapBlock* Heap::findFreeBlock(size_t size) {
    HeapBlock* current = firstBlock;
    
    while (current) {
        if (current->free && current->size >= size && current->isValid()) {
            return current;
        }
        current = current->next;
    }
    
    return nullptr;
}

void Heap::splitBlock(HeapBlock* block, size_t size) {
    if (block->size >= size + sizeof(HeapBlock) + 16) {
        HeapBlock* newBlock = reinterpret_cast<HeapBlock*>(
            reinterpret_cast<uint64_t>(block->getData()) + size
        );
        
        newBlock->size = block->size - size - sizeof(HeapBlock);
        newBlock->free = true;
        newBlock->next = block->next;
        newBlock->prev = block;
        newBlock->magic = HeapBlock::defaultMagic;
        
        if (block->next) {
            block->next->prev = newBlock;
        }
        
        block->next = newBlock;
        block->size = size;
    }
}

void Heap::mergeBlocks(HeapBlock* block) {
    if (!block || !block->isValid()) return;

    if (block->next && block->next->free && block->next->isValid()) {
        block->size += sizeof(HeapBlock) + block->next->size;
        block->next = block->next->next;
        
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->free && block->prev->isValid()) {
        block->prev->size += sizeof(HeapBlock) + block->size;
        block->prev->next = block->next;
        
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void* Heap::allocate(size_t size) {
    if (!initialized || size == 0) return nullptr;

    size = alignSize(size);

    HeapBlock* block = findFreeBlock(size);

    if (!block) {
        if (!expand(size + sizeof(HeapBlock))) {
            return nullptr;
        }
        block = findFreeBlock(size);
        if (!block) return nullptr;
    }
    
    splitBlock(block, size);
    
    block->free = false;
    usedSize += block->size + sizeof(HeapBlock);
    
    return block->getData();
}

void* Heap::allocateAligned(size_t size, size_t alignment) {
    if (!initialized || size == 0) return nullptr;
    
    size_t total_size = size + alignment + sizeof(void*);
    void* ptr = allocate(total_size);
    
    if (!ptr) return nullptr;
    
    uint64_t addr = reinterpret_cast<uint64_t>(ptr);
    uint64_t aligned = (addr + sizeof(void*) + alignment - 1) & ~(alignment - 1);
    
    void** original_ptr = reinterpret_cast<void**>(aligned - sizeof(void*));
    *original_ptr = ptr;
    
    return reinterpret_cast<void*>(aligned);
}

void Heap::free(void* ptr) {
    if (!initialized || !ptr) return;
    
    HeapBlock* block = HeapBlock::fromData(ptr);
    
    if (!block->isValid() || block->free) {
        return;
    }
    
    block->free = true;
    usedSize -= block->size + sizeof(HeapBlock);
    
    mergeBlocks(block);
}

void* Heap::reallocate(void* ptr, size_t newSize) {
    if (!ptr) {
        return allocate(newSize);
    }
    
    if (newSize == 0) {
        free(ptr);
        return nullptr;
    }
    
    HeapBlock* block = HeapBlock::fromData(ptr);
    
    if (!block->isValid()) {
        return nullptr;
    }
    
    newSize = alignSize(newSize);
    if (block->size >= newSize) {
        return ptr;
    }
    
    void* pointer = allocate(newSize);
    if (!pointer) {
        return nullptr;
    }
    
    size_t copy_size = block->size < newSize ? block->size : newSize;
    uint8_t* src = reinterpret_cast<uint8_t*>(ptr);
    uint8_t* dst = reinterpret_cast<uint8_t*>(pointer);
    
    for (size_t i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }
    
    free(ptr);
    
    return pointer;
}

bool Heap::expand(size_t finalSize) {
    size_t _pages = (finalSize + PAGE_SIZE - 1) / PAGE_SIZE;
    
    void* phys = pmm.allocatePages(_pages);
    if (!phys) {
        return false;
    }
    
    void* virt = endLocation;
    if (!vmm.mapRange(virt, phys, _pages, PTE_PRESENT | PTE_WRITABLE)) {
        pmm.freePages(phys, _pages);
        return false;
    }
    
    HeapBlock* newBlock = reinterpret_cast<HeapBlock*>(endLocation);
    newBlock->size = _pages * PAGE_SIZE - sizeof(HeapBlock);
    newBlock->free = true;
    newBlock->next = nullptr;
    newBlock->magic = HeapBlock::defaultMagic;
    
    HeapBlock* last = firstBlock;
    while (last->next) {
        last = last->next;
    }

    last->next = newBlock;
    newBlock->prev = last;

    endLocation = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(endLocation) + _pages * PAGE_SIZE);
    totalSize += _pages * PAGE_SIZE;

    mergeBlocks(newBlock);
    
    return true;
}