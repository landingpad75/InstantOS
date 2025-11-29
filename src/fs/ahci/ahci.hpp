#pragma once

#include <cstdint>
#include <fs/fat32/fat32.hpp>

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SATAPI 3
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 15

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_FR 0x4000
#define HBA_PxCMD_CR 0x8000

#define HBA_PxIS_TFES (1 << 30)

struct HBAPort {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed));

struct HBAMemory {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t rsv[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    HBAPort ports[32];
} __attribute__((packed));

struct HBACmdHeader {
    uint8_t cfl : 5;
    uint8_t a : 1;
    uint8_t w : 1;
    uint8_t p : 1;
    uint8_t r : 1;
    uint8_t b : 1;
    uint8_t c : 1;
    uint8_t rsv0 : 1;
    uint8_t pmp : 4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} __attribute__((packed));

struct HBAPRDTEntry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc : 22;
    uint32_t rsv1 : 9;
    uint32_t i : 1;
} __attribute__((packed));

struct HBACmdTbl {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    HBAPRDTEntry prdt_entry[1];
} __attribute__((packed));

struct FISRegH2D {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t rsv1[4];
} __attribute__((packed));

class AHCIPort {
public:
    AHCIPort(HBAPort* port, int portNum);
    
    bool initialize();
    bool read(uint64_t sector, uint32_t count, void* buffer);
    bool write(uint64_t sector, uint32_t count, const void* buffer);
    bool identify(uint16_t* buffer);
    
    int getType();
    bool isActive() { return active; }
    uint64_t getSectorCount() { return sectorCount; }
    
private:
    HBAPort* port;
    int portNum;
    bool active;
    uint64_t sectorCount;
    
    void startCmd();
    void stopCmd();
    int findCmdSlot();
    bool executeCommand(uint8_t cmd, uint64_t lba, uint32_t count, void* buffer, bool write);
};

class AHCIController {
public:
    AHCIController(uint64_t abar);
    ~AHCIController();
    
    bool initialize();
    int getPortCount() { return portCount; }
    AHCIPort* getPort(int index);
    
private:
    HBAMemory* hba;
    AHCIPort* ports[32];
    int portCount;
    uint64_t abar;
};

class SATABlockDevice : public BlockDevice {
public:
    SATABlockDevice(AHCIPort* port);
    ~SATABlockDevice() override;
    
    bool read(uint64_t offset, void* buffer, uint64_t size) override;
    bool write(uint64_t offset, const void* buffer, uint64_t size) override;
    uint64_t getSize() override;
    
private:
    AHCIPort* port;
    uint64_t totalSize;
};
