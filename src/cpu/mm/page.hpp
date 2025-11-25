#pragma once

#include <cstdint>

struct PageTableEntry {
    uint64_t value;
    
    void setAddress(uint64_t addr) {
        value = (value & 0xFFF0000000000FFF) | (addr & 0x000FFFFFFFFFF000);
    }
    
    uint64_t getAddress() const {
        return value & 0x000FFFFFFFFFF000;
    }
    
    void setFlags(uint64_t flags) {
        value = (value & 0x000FFFFFFFFFF000) | (flags & 0xFFF0000000000FFF);
    }
    
    void addFlags(uint64_t flags) {
        value |= flags;
    }
    
    void removeFlags(uint64_t flags) {
        value &= ~flags;
    }
    
    bool hasFlag(uint64_t flag) const {
        return (value & flag) != 0;
    }
    
    void clear() {
        value = 0;
    }
};

struct PageTable {
    PageTableEntry entries[512];
} __attribute__((aligned(4096)));