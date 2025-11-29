#include "ahci.hpp"
#include <cpu/mm/heap.hpp>
#include <cpu/mm/pmm.hpp>
#include <cpu/mm/vmm.hpp>
#include <x86_64/requests.hpp>
#include <cstddef>

extern Heap kheap;
extern PMM pmm;
extern VMM vmm;

AHCIPort::AHCIPort(HBAPort* port, int portNum) : port(port), portNum(portNum), active(false), sectorCount(0) {
}

int AHCIPort::getType() {
    uint32_t ssts = port->ssts;
    
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    
    if (det != HBA_PORT_DET_PRESENT) return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;
    
    switch (port->sig) {
        case 0xEB140101:
            return AHCI_DEV_SATAPI;
        case 0xC33C0101:
            return AHCI_DEV_SEMB;
        case 0x96690101:
            return AHCI_DEV_PM;
        default:
            return AHCI_DEV_SATA;
    }
}

void AHCIPort::stopCmd() {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    
    while (true) {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
}

void AHCIPort::startCmd() {
    while (port->cmd & HBA_PxCMD_CR);
    
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

int AHCIPort::findCmdSlot() {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & 1) == 0) {
            return i;
        }
        slots >>= 1;
    }
    return -1;
}


bool AHCIPort::initialize() {
    stopCmd();
    
    void* clbPhys = pmm.allocatePage();
    if (!clbPhys) return false;
    uint64_t clbVirt = (uint64_t)clbPhys + hhdm_request.response->offset;
    
    void* fbPhys = pmm.allocatePage();
    if (!fbPhys) {
        pmm.freePage(clbPhys);
        return false;
    }
    uint64_t fbVirt = (uint64_t)fbPhys + hhdm_request.response->offset;
    
    for (int i = 0; i < 1024; i++) {
        ((uint8_t*)clbVirt)[i] = 0;
    }
    for (int i = 0; i < 256; i++) {
        ((uint8_t*)fbVirt)[i] = 0;
    }
    
    port->clb = (uint64_t)clbPhys & 0xFFFFFFFF;
    port->clbu = ((uint64_t)clbPhys >> 32) & 0xFFFFFFFF;
    port->fb = (uint64_t)fbPhys & 0xFFFFFFFF;
    port->fbu = ((uint64_t)fbPhys >> 32) & 0xFFFFFFFF;
    
    HBACmdHeader* cmdheader = (HBACmdHeader*)clbVirt;
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8;
        
        void* ctbPhys = pmm.allocatePage();
        if (!ctbPhys) continue;
        uint64_t ctbVirt = (uint64_t)ctbPhys + hhdm_request.response->offset;
        
        for (int j = 0; j < 256; j++) {
            ((uint8_t*)ctbVirt)[j] = 0;
        }
        
        cmdheader[i].ctba = (uint64_t)ctbPhys & 0xFFFFFFFF;
        cmdheader[i].ctbau = ((uint64_t)ctbPhys >> 32) & 0xFFFFFFFF;
    }
    
    startCmd();
    
    uint16_t* identifyBuffer = (uint16_t*)kheap.allocate(512);
    if (identifyBuffer && identify(identifyBuffer)) {
        sectorCount = *(uint64_t*)&identifyBuffer[100];
        if (sectorCount == 0) {
            sectorCount = *(uint32_t*)&identifyBuffer[60];
        }
        kheap.free(identifyBuffer);
    }
    
    active = true;
    return true;
}

bool AHCIPort::executeCommand(uint8_t cmd, uint64_t lba, uint32_t count, void* buffer, bool isWrite) {
    port->is = (uint32_t)-1;
    
    int slot = findCmdSlot();
    if (slot == -1) return false;
    
    uint64_t clbVirt = ((uint64_t)port->clb | ((uint64_t)port->clbu << 32)) + hhdm_request.response->offset;
    HBACmdHeader* cmdheader = (HBACmdHeader*)clbVirt;
    
    cmdheader += slot;
    cmdheader->cfl = sizeof(FISRegH2D) / sizeof(uint32_t);
    cmdheader->w = isWrite ? 1 : 0;
    cmdheader->prdtl = (uint16_t)((count - 1) / 8) + 1;
    
    uint64_t ctbVirt = ((uint64_t)cmdheader->ctba | ((uint64_t)cmdheader->ctbau << 32)) + hhdm_request.response->offset;
    HBACmdTbl* cmdtbl = (HBACmdTbl*)ctbVirt;
    
    for (int i = 0; i < 256; i++) {
        ((uint8_t*)cmdtbl)[i] = 0;
    }
    
    uint64_t bufferPhys = (uint64_t)buffer - hhdm_request.response->offset;
    int i;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        cmdtbl->prdt_entry[i].dba = bufferPhys & 0xFFFFFFFF;
        cmdtbl->prdt_entry[i].dbau = (bufferPhys >> 32) & 0xFFFFFFFF;
        cmdtbl->prdt_entry[i].dbc = 8 * 1024 - 1;
        cmdtbl->prdt_entry[i].i = 1;
        bufferPhys += 8 * 1024;
        count -= 16;
    }
    
    cmdtbl->prdt_entry[i].dba = bufferPhys & 0xFFFFFFFF;
    cmdtbl->prdt_entry[i].dbau = (bufferPhys >> 32) & 0xFFFFFFFF;
    cmdtbl->prdt_entry[i].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[i].i = 1;
    
    FISRegH2D* cmdfis = (FISRegH2D*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = cmd;
    
    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6;
    
    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);
    
    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;
    
    int spin = 0;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) return false;
    
    port->ci = 1 << slot;
    
    while (true) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return false;
    }
    
    if (port->is & HBA_PxIS_TFES) return false;
    
    return true;
}

bool AHCIPort::read(uint64_t sector, uint32_t count, void* buffer) {
    return executeCommand(ATA_CMD_READ_DMA_EX, sector, count, buffer, false);
}

bool AHCIPort::write(uint64_t sector, uint32_t count, const void* buffer) {
    return executeCommand(ATA_CMD_WRITE_DMA_EX, sector, count, (void*)buffer, true);
}

bool AHCIPort::identify(uint16_t* buffer) {
    return executeCommand(ATA_CMD_IDENTIFY, 0, 1, buffer, false);
}


AHCIController::AHCIController(uint64_t abar) : hba(nullptr), portCount(0), abar(abar) {
    for (int i = 0; i < 32; i++) {
        ports[i] = nullptr;
    }
}

AHCIController::~AHCIController() {
    for (int i = 0; i < 32; i++) {
        if (ports[i]) {
            delete ports[i];
        }
    }
}

bool AHCIController::initialize() {
    size_t abar_size = sizeof(HBAMemory);
    size_t pages_needed = (abar_size + 4095) / 4096;
    if (pages_needed < 2) pages_needed = 2;
    
    uint64_t abar_aligned = abar & ~0xFFF;
    uint64_t hhdm_offset = hhdm_request.response->offset;
    
    for (size_t i = 0; i < pages_needed; i++) {
        void* phys = (void*)(abar_aligned + i * 4096);
        void* virt = (void*)(abar_aligned + i * 4096 + hhdm_offset);
        vmm.map(virt, phys, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
    }
    
    hba = (HBAMemory*)(abar + hhdm_offset);
    
    uint32_t pi = hba->pi;
    
    for (int i = 0; i < 32; i++) {
        if (pi & 1) {
            AHCIPort* port = new AHCIPort(&hba->ports[i], i);
            int type = port->getType();
            
            if (type == AHCI_DEV_SATA) {
                if (port->initialize()) {
                    ports[portCount++] = port;
                } else {
                    delete port;
                }
            } else {
                delete port;
            }
        }
        pi >>= 1;
    }
    
    return portCount > 0;
}

AHCIPort* AHCIController::getPort(int index) {
    if (index < 0 || index >= portCount) return nullptr;
    return ports[index];
}

SATABlockDevice::SATABlockDevice(AHCIPort* port) : port(port), totalSize(0) {
    if (port) {
        totalSize = port->getSectorCount() * 512;
    }
}

SATABlockDevice::~SATABlockDevice() {
}

bool SATABlockDevice::read(uint64_t offset, void* buffer, uint64_t size) {
    if (!port || !buffer) return false;
    
    uint64_t sector = offset / 512;
    uint64_t sectorOffset = offset % 512;
    uint32_t sectorCount = (size + sectorOffset + 511) / 512;
    
    if (sectorOffset == 0 && size % 512 == 0) {
        return port->read(sector, sectorCount, buffer);
    }
    
    void* tempBuffer = kheap.allocate(sectorCount * 512);
    if (!tempBuffer) return false;
    
    if (!port->read(sector, sectorCount, tempBuffer)) {
        kheap.free(tempBuffer);
        return false;
    }
    
    uint8_t* src = (uint8_t*)tempBuffer + sectorOffset;
    uint8_t* dst = (uint8_t*)buffer;
    for (uint64_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    kheap.free(tempBuffer);
    return true;
}

bool SATABlockDevice::write(uint64_t offset, const void* buffer, uint64_t size) {
    if (!port || !buffer) return false;
    
    uint64_t sector = offset / 512;
    uint64_t sectorOffset = offset % 512;
    uint32_t sectorCount = (size + sectorOffset + 511) / 512;
    
    if (sectorOffset == 0 && size % 512 == 0) {
        return port->write(sector, sectorCount, buffer);
    }
    
    void* tempBuffer = kheap.allocate(sectorCount * 512);
    if (!tempBuffer) return false;
    
    if (sectorOffset != 0 || (size % 512) != 0) {
        if (!port->read(sector, sectorCount, tempBuffer)) {
            kheap.free(tempBuffer);
            return false;
        }
    }
    
    uint8_t* dst = (uint8_t*)tempBuffer + sectorOffset;
    const uint8_t* src = (const uint8_t*)buffer;
    for (uint64_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    bool result = port->write(sector, sectorCount, tempBuffer);
    kheap.free(tempBuffer);
    return result;
}

uint64_t SATABlockDevice::getSize() {
    return totalSize;
}
