#include "memory.h"

#include <stdio.h>
#include <string.h>

#define RAM_PHYSICAL_LIMIT 0x00800000u
#define KSEG_PHYSICAL_MASK 0x1FFFFFFFu

static uint32_t normalize_address(uint32_t address)
{
    if (address >= 0x80000000u && address <= 0xBFFFFFFFu)
    {
        return address & KSEG_PHYSICAL_MASK;
    }

    return address;
}

static bool translate_ram_address(uint32_t address, uint32_t *offset)
{
    address = normalize_address(address);

    if (address >= RAM_PHYSICAL_LIMIT)
    {
        return false;
    }

    *offset = address % RAM_SIZE;
    return true;
}

static bool translate_bios_address(uint32_t address, uint32_t *offset)
{
    address = normalize_address(address);

    if (address < BIOS_PHYSICAL_START || address >= BIOS_PHYSICAL_END)
    {
        return false;
    }

    *offset = address - BIOS_PHYSICAL_START;
    return true;
}

void memory_init(Memory *memory)
{
    memset(memory, 0, sizeof(*memory));
}

bool memory_load_bios(Memory *memory, const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return false;
    }

    // Check that the BIOS file has the expected size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }

    long file_size = ftell(file);

    if (file_size != (long)BIOS_SIZE)
    {
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(memory->bios, 1, BIOS_SIZE, file);
    bool success = bytes_read == BIOS_SIZE;

    if (fclose(file) != 0)
    {
        success = false;
    }

    return success;
}

bool memory_read8(const Memory *memory, uint32_t address, uint8_t *value)
{
    uint32_t offset;

    if (translate_ram_address(address, &offset))
    {
        *value = memory->data[offset];
        return true;
    }

    if (translate_bios_address(address, &offset))
    {
        *value = memory->bios[offset];
        return true;
    }

    return false;
}

bool memory_write8(Memory *memory, uint32_t address, uint8_t value)
{
    uint32_t offset;

    if (!translate_ram_address(address, &offset))
    {
        /*
         * The BIOS and unimplemented regions are read-only
         * or unavailable in this memory model.
         */
        return false;
    }

    memory->data[offset] = value;

    return true;
}

bool memory_read32(const Memory *memory, uint32_t address, uint32_t *value)
{
    if ((address & 0x3u) != 0)
    {
        return false;
    }

    uint32_t physical_address = normalize_address(address);

    switch (physical_address)
    {
        case EXP1_BASE_ADDRESS:
            *value = memory->exp1_base;
            return true;

        case EXP2_BASE_ADDRESS:
            *value = memory->exp2_base;
            return true;

        case EXP1_DELAY_ADDRESS:
            *value = memory->exp1_delay;
            return true;

        case EXP3_DELAY_ADDRESS:
            *value = memory->exp3_delay;
            return true;

        case BIOS_DELAY_ADDRESS:
            *value = memory->bios_delay;
            return true;

        case SPU_DELAY_ADDRESS:
            *value = memory->spu_delay;
            return true;

        case CDROM_DELAY_ADDRESS:
            *value = memory->cdrom_delay;
            return true;

        case EXP2_DELAY_ADDRESS:
            *value = memory->exp2_delay;
            return true;

        case COMMON_DELAY_ADDRESS:
            *value = memory->common_delay;
            return true;

        case DRAM_CTRL_ADDRESS:
            *value = memory->dram_ctrl;
            return true;

        case DMA0_MADR_ADDRESS:
            *value = memory->dma0_madr;
            return true;

        case DMA0_BCR_ADDRESS:
            *value = memory->dma0_bcr;
            return true;

        case DMA0_CHCR_ADDRESS:
            *value = memory->dma0_chcr;
            return true;

        case DMA_DPCR_ADDRESS:
            *value = memory->dma_dpcr;
            return true;

        default:
            break;
    }

    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;

    if (!memory_read8(memory, address, &b0) || !memory_read8(memory, address + 1, &b1) || !memory_read8(memory, address + 2, &b2) || !memory_read8(memory, address + 3, &b3))
    {
        return false;
    }

    *value = (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);

    return true;
}

bool memory_write32(Memory *memory, uint32_t address, uint32_t value)
{
    if ((address & 0x3u) != 0)
    {
        return false;
    }

    uint32_t physical_address = normalize_address(address);

    switch (physical_address)
    {
        case EXP1_BASE_ADDRESS:
            memory->exp1_base = value;
            return true;

        case EXP2_BASE_ADDRESS:
            memory->exp2_base = value;
            return true;

        case EXP1_DELAY_ADDRESS:
            memory->exp1_delay = value;
            return true;

        case EXP3_DELAY_ADDRESS:
            memory->exp3_delay = value;
            return true;

        case BIOS_DELAY_ADDRESS:
            memory->bios_delay = value;
            return true;

        case SPU_DELAY_ADDRESS:
            memory->spu_delay = value;
            return true;

        case CDROM_DELAY_ADDRESS:
            memory->cdrom_delay = value;
            return true;

        case EXP2_DELAY_ADDRESS:
            memory->exp2_delay = value;
            return true;

        case COMMON_DELAY_ADDRESS:
            memory->common_delay = value;
            return true;

        case DRAM_CTRL_ADDRESS:
            memory->dram_ctrl = value;
            return true;

        case DMA0_MADR_ADDRESS:
            memory->dma0_madr = value;
            return true;

        case DMA0_BCR_ADDRESS:
            memory->dma0_bcr = value;
            return true;

        case DMA0_CHCR_ADDRESS:
            memory->dma0_chcr = value;
            return true;

        case DMA_DPCR_ADDRESS:
            memory->dma_dpcr = value;
            return true;

        default:
            break;
    }

    // Validate the entire word before writing any bytes
    uint32_t offset;

    if (!translate_ram_address(address, &offset) || offset > RAM_SIZE - 4)
    {
        return false;
    }

    memory->data[offset] = (uint8_t)(value & 0xFFu);
    memory->data[offset + 1] = (uint8_t)((value >> 8) & 0xFFu);
    memory->data[offset + 2] = (uint8_t)((value >> 16) & 0xFFu);
    memory->data[offset + 3] = (uint8_t)((value >> 24) & 0xFFu);

    return true;
}