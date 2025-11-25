#include "heap.hpp"
#include <cstddef>

void* operator new(size_t size) {
    return kheap.allocate(size);
}

void* operator new[](size_t size) {
    return kheap.allocate(size);
}

void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}

void* operator new[](size_t, void* ptr) noexcept {
    return ptr;
}

void* operator new(size_t size, std::align_val_t align) {
    return kheap.allocateAligned(size, static_cast<size_t>(align));
}

void* operator new[](size_t size, std::align_val_t align) {
    return kheap.allocateAligned(size, static_cast<size_t>(align));
}

void operator delete(void* ptr) noexcept {
    kheap.free(ptr);
}

void operator delete[](void* ptr) noexcept {
    kheap.free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    kheap.free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    kheap.free(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept {
    kheap.free(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept {
    kheap.free(ptr);
}

void operator delete(void* ptr, size_t, std::align_val_t) noexcept {
    kheap.free(ptr);
}

void operator delete[](void* ptr, size_t, std::align_val_t) noexcept {
    kheap.free(ptr);
}