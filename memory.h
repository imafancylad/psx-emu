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

#define I_STAT_ADDRESS 0x1F801070u   /* Interrupt Status (write-1-to-clear) */
#define I_MASK_ADDRESS 0x1F801074u   /* Interrupt Mask */

#define TIMER0_BASE 0x1F801100u
#define TIMER1_BASE 0x1F801110u
#define TIMER2_BASE 0x1F801120u

#define GP0_ADDRESS    0x1F801810u   /* write = GP0 command, read = GPUREAD */
#define GP1_ADDRESS    0x1F801814u   /* write = GP1 control, read = GPUSTAT */

#define DMA_DICR_ADDRESS  0x1F8010F4u

#define DMA1_MADR_ADDRESS 0x1F801090u
#define DMA1_BCR_ADDRESS  0x1F801094u
#define DMA1_CHCR_ADDRESS 0x1F801098u

#define DMA2_MADR_ADDRESS 0x1F8010A0u
#define DMA2_BCR_ADDRESS  0x1F8010A4u
#define DMA2_CHCR_ADDRESS 0x1F8010A8u

#define DMA3_MADR_ADDRESS 0x1F8010B0u
#define DMA3_BCR_ADDRESS  0x1F8010B4u
#define DMA3_CHCR_ADDRESS 0x1F8010B8u

#define DMA4_MADR_ADDRESS 0x1F8010C0u
#define DMA4_BCR_ADDRESS  0x1F8010C4u
#define DMA4_CHCR_ADDRESS 0x1F8010C8u

#define DMA5_MADR_ADDRESS 0x1F8010D0u
#define DMA5_BCR_ADDRESS  0x1F8010D4u
#define DMA5_CHCR_ADDRESS 0x1F8010D8u

#define DMA6_MADR_ADDRESS 0x1F8010E0u
#define DMA6_BCR_ADDRESS  0x1F8010E4u
#define DMA6_CHCR_ADDRESS 0x1F8010E8u

#define CDROM_BASE 0x1F801800u

typedef struct {
    uint8_t data[RAM_SIZE];
    uint8_t bios[BIOS_SIZE];
    uint8_t scratchpad[SCRATCHPAD_SIZE];
    uint8_t exp2_memory[EXP2_MEMORY_SIZE];

    uint8_t cdrom_index;
    uint8_t cdrom_status;
    uint8_t cdrom_command;
    uint8_t cdrom_param[16];
    uint8_t cdrom_response[16];
    uint8_t cdrom_irq_enable;
    uint8_t cdrom_irq_flags;
    uint8_t cdrom_param_count;
    uint8_t cdrom_response_count;

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

    uint32_t dma1_madr, dma1_bcr, dma1_chcr;
    uint32_t dma2_madr, dma2_bcr, dma2_chcr;
    uint32_t dma3_madr, dma3_bcr, dma3_chcr;
    uint32_t dma4_madr, dma4_bcr, dma4_chcr;
    uint32_t dma5_madr, dma5_bcr, dma5_chcr;
    uint32_t dma6_madr, dma6_bcr, dma6_chcr;
    uint32_t dma_dicr;
    uint32_t bcc;

    uint32_t i_stat;
    uint32_t i_mask;

    uint16_t timer_counter[3];
    uint16_t timer_mode[3];
    uint16_t timer_target[3];
    uint32_t timer_cycles[3];   /* ciclos acumulados desde ultimo tick */

    uint32_t gp0_command;
    uint32_t gp1_command;
    uint32_t gpu_read;
    uint32_t gpu_stat;
} Memory;

void memory_init(Memory *memory);

bool memory_load_bios(Memory *memory, const char *path);
bool memory_read8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write8(Memory *memory, uint32_t address, uint8_t value);
bool memory_read32(const Memory *memory, uint32_t address, uint32_t *value);
bool memory_write32(Memory *memory, uint32_t address, uint32_t value);
bool memory_write16(Memory *memory, uint32_t address, uint16_t value);
bool memory_read16(Memory *memory, uint32_t address, uint16_t *value);

#endif