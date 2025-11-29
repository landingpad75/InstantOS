#include "detect.hpp"
#include <x86_64/ports.hpp>

uint32_t AHCIDetector::pciConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void AHCIDetector::pciConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

uint16_t AHCIDetector::pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pciConfigReadDword(bus, slot, func, offset & 0xFC);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t AHCIDetector::pciConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pciConfigReadDword(bus, slot, func, offset & 0xFC);
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

bool AHCIDetector::checkDevice(uint8_t bus, uint8_t slot, uint8_t func, uint64_t* abar) {
    uint16_t vendorID = pciConfigReadWord(bus, slot, func, PCI_VENDOR_ID);
    if (vendorID == 0xFFFF) return false;
    
    uint8_t classCode = pciConfigReadByte(bus, slot, func, PCI_CLASS);
    uint8_t subclass = pciConfigReadByte(bus, slot, func, PCI_SUBCLASS);
    uint8_t progIF = pciConfigReadByte(bus, slot, func, PCI_PROG_IF);
    
    if (classCode == PCI_CLASS_MASS_STORAGE && subclass == PCI_SUBCLASS_SATA && progIF == PCI_PROG_IF_AHCI) {
        uint32_t bar5 = pciConfigReadDword(bus, slot, func, PCI_BAR5);
        *abar = bar5 & 0xFFFFFFF0;
        
        uint16_t command = pciConfigReadWord(bus, slot, func, PCI_COMMAND);
        command |= 0x06;
        pciConfigWriteDword(bus, slot, func, PCI_COMMAND, command);
        
        return true;
    }
    
    return false;
}

AHCIController* AHCIDetector::detectAndInitialize() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint64_t abar;
                if (checkDevice(bus, slot, func, &abar)) {
                    AHCIController* controller = new AHCIController(abar);
                    if (controller->initialize()) {
                        return controller;
                    }
                    delete controller;
                }
            }
        }
    }
    
    return nullptr;
}
