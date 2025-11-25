#include "pmm.hpp"
PMM pmm;

void PMM::init(uint8_t* bmpBuffer, uint64_t maxMemory) {
    availableMemory = maxMemory;
    pages = maxMemory / PAGE_SIZE;
    usedMemory = 0;
    freeMemory = maxMemory;
    
    bitmap.init(bmpBuffer, pages);

    intialized = true;
}

void* PMM::allocatePage() {
    if (!intialized) return nullptr;

    size_t index = bitmap.findFirstFree();
    if (index >= pages) {
        return nullptr;  // Out of memory
    }

    bitmap.set(index);
    usedMemory += PAGE_SIZE;
    freeMemory -= PAGE_SIZE;

    return indexToAddress(index);
}

void* PMM::allocatePages(size_t count) {
    if (!intialized || count == 0) return nullptr;

    size_t index = bitmap.findFirstFreeRegion(count);
    if (index >= pages) {
        return nullptr;  // Not enough contigous memory
    }

    bitmap.setRange(index, count);
    usedMemory += count * PAGE_SIZE;
    freeMemory -= count * PAGE_SIZE;
    
    return indexToAddress(index);
}

void PMM::freePage(void* page) {
    if (!intialized || !page) return;
    
    size_t index = addressToIndex(page);
    if (index >= pages) return;

    if (bitmap.get(index)) {
        bitmap.clear(index);
        usedMemory -= PAGE_SIZE;
        freeMemory += PAGE_SIZE;
    }
}

void PMM::freePages(void* page, size_t count) {
    if (!intialized || !page || count == 0) return;

    size_t index = addressToIndex(page);
    if (index >= pages) return;

    for (size_t i = 0; i < count; i++) {
        if (index + i < pages && bitmap.get(index + i)) {
            bitmap.clear(index + i);
            usedMemory -= PAGE_SIZE;
            freeMemory += PAGE_SIZE;
        }
    }
}

void PMM::reservePage(void* page) {
    if (!intialized || !page) return;

    size_t index = addressToIndex(page);
    if (index >= pages) return;

    if (!bitmap.get(index)) {
        bitmap.set(index);
        usedMemory += PAGE_SIZE;
        freeMemory -= PAGE_SIZE;
    }
}

void PMM::reservePages(void* page, size_t count) {
    if (!intialized || !page || count == 0) return;

    size_t index = addressToIndex(page);
    if (index >= pages) return;

    for (size_t i = 0; i < count; i++) {
        if (index + i < pages && !bitmap.get(index + i)) {
            bitmap.set(index + i);
            usedMemory += PAGE_SIZE;
            freeMemory -= PAGE_SIZE;
        }
    }
}

void PMM::reserveRegion(uint64_t base, uint64_t length) {
    if (!intialized || length == 0) return;

    uint64_t aligned_base = base & ~(PAGE_SIZE - 1);
    size_t page_count = (length + (base - aligned_base) + PAGE_SIZE - 1) / PAGE_SIZE;
    reservePages(reinterpret_cast<void*>(aligned_base), page_count);
}