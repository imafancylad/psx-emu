#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#define RAM_SIZE (2 * 1024 * 1024)
#define BIOS_SIZE (512 * 1024)

#define SCRATCHPAD_SIZE 1024
#define SCRATCHPAD_START 0x1F800000u
#define SCRATCHPAD_END   0x1F800400u

#define SPU_REGISTERS_START 0x1F801C00u
#define SPU_REGISTERS_END   0x1F801E00u
#define SPU_REGISTERS_COUNT 256

#define BIOS_PHYSICAL_START 0x1FC00000u
#define BIOS_PHYSICAL_END   0x1FC80000u

/* Memory control registers */
#define EXP1_BASE_ADDRESS  0x1F801000u
#define EXP2_BASE_ADDRESS  0x1F801004u
#define EXP1_DELAY_ADDRESS 0x1F801008u
#define EXP3_DELAY_ADDRESS 0x1F80100Cu
#define BIOS_DELAY_ADDRESS 0x1F801010u
#define SPU_DELAY_ADDRESS  0x1F801014u
#define CDROM_DELAY_ADDRESS 0x1F801018u
#define EXP2_DELAY_ADDRESS 0x1F80101Cu
#define COMMON_DELAY_ADDRESS 0x1F801020u
#define DRAM_CTRL_ADDRESS  0x1F801060u

#define EXP2_MEMORY_START 0x1F802000u
#define EXP2_MEMORY_END   0x1F804000u
#define EXP2_MEMORY_SIZE  (EXP2_MEMORY_END - EXP2_MEMORY_START)

#define EXP1_MEMORY_START 0x1F000000u
#define EXP1_MEMORY_END   0x1F800000u

/* DMA registers */
#define DMA0_MADR_ADDRESS 0x1F801080u
#define DMA0_BCR_ADDRESS  0x1F801084u
#define DMA0_CHCR_ADDRESS 0x1F801088u
#define DMA_DPCR_ADDRESS  0x1F8010F0u
#define BCC_ADDRESS 0xFFFE0130u

typedef struct {
    uint8_t data[RAM_SIZE];
    uint8_t bios[BIOS_SIZE];
    uint8_t scratchpad[SCRATCHPAD_SIZE];
    uint8_t exp2_memory[EXP2_MEMORY_SIZE];

    uint16_t spu_registers[SPU_REGISTERS_COUNT];

    uint32_t exp1_base;
    uint32_t exp2_base;
    uint32_t exp1_delay;
    uint32_t exp3_delay;
    uint32_t bios_delay;
    uint32_t spu_delay;
    uint32_t cdrom_delay;
    uint32_t exp2_delay;
    uint32_t common_delay;
    uint32_t dram_ctrl;

    uint32_t dma0_madr;
    uint32_t dma0_bcr;
    uint32_t dma0_chcr;
    uint32_t dma_dpcr;
    uint32_t bcc;
} Memory;

void memory_init(Memory *memory);

bool memory_load_bios(Memory *memory, const char *path);
bool memory_read8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write8(Memory *memory, uint32_t address, uint8_t value);
bool memory_read32(const Memory *memory, uint32_t address, uint32_t *value);
bool memory_write32(Memory *memory, uint32_t address, uint32_t value);
bool memory_write16(Memory *memory, uint32_t address, uint16_t value);

#endif