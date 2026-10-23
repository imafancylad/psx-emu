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

static bool translate_scratchpad_address(uint32_t address, uint32_t *offset)
{
    address = normalize_address(address);

    if (address < SCRATCHPAD_START || address >= SCRATCHPAD_END)
    {
        return false;
    }

    *offset = address - SCRATCHPAD_START;
    return true;
}

void memory_init(Memory *memory)
{
    memset(memory, 0, sizeof(*memory));
    memory->gpu_stat = 0x14802000u;
}

bool memory_load_bios(Memory *memory, const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return false;
    }

    // verifica si el archivo de BIOS tiene el tamaño correspondiente
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
    uint32_t physical_address = normalize_address(address);

    if (physical_address >= EXP1_MEMORY_START && physical_address < EXP1_MEMORY_END)
    {
        *value = 0;
        return true;
    }

    if (physical_address >= SPU_REGISTERS_START && physical_address < SPU_REGISTERS_END)
    {
        uint32_t spu_offset = physical_address - SPU_REGISTERS_START;
        uint16_t reg = memory->spu_registers[spu_offset / 2];

        if ((spu_offset & 1u) == 0)
        {
            *value = (uint8_t)(reg & 0xFFu);
        }
        else
        {
            *value = (uint8_t)((reg >> 8) & 0xFFu);
        }

        return true;
    }

    if (physical_address >= EXP2_MEMORY_START && physical_address < EXP2_MEMORY_END)
    {
        *value = memory->exp2_memory[physical_address - EXP2_MEMORY_START];
        return true;
    }

    if (translate_scratchpad_address(address, &offset))
    {
        *value = memory->scratchpad[offset];
        return true;
    }

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

    if (physical_address >= 0x1F801000u && physical_address < 0x1F804000u)
    {
        *value = 0;
        return true;
    }

    if (physical_address >= 0xC0000000u)
    {
        *value = 0;
        return true;
    }

    return false;
}

bool memory_write8(Memory *memory, uint32_t address, uint8_t value)
{
    uint32_t offset;
    uint32_t physical_address = normalize_address(address);

    if (physical_address >= EXP1_MEMORY_START && physical_address < EXP1_MEMORY_END)
    {
        return true;
    }

    if (physical_address >= SPU_REGISTERS_START && physical_address < SPU_REGISTERS_END)
    {
        uint32_t spu_offset = physical_address - SPU_REGISTERS_START;
        uint16_t *reg = &memory->spu_registers[spu_offset / 2];

        if ((spu_offset & 1u) == 0)
        {
            *reg = (uint16_t)((*reg & 0xFF00u) | value);
        }
        else
        {
            *reg = (uint16_t)((*reg & 0x00FFu) | ((uint16_t)value << 8));
        }

        return true;
    }

    if (physical_address >= EXP2_MEMORY_START && physical_address < EXP2_MEMORY_END)
    {
        memory->exp2_memory[physical_address - EXP2_MEMORY_START] = value;
        return true;
    }

    if (translate_scratchpad_address(address, &offset))
    {
        memory->scratchpad[offset] = value;
        return true;
    }

        if (translate_ram_address(address, &offset))
    {
        memory->data[offset] = value;
        return true;
    }

    /* MMIO no diseñada, ignora escritura en vez de fallar */
    if (physical_address >= 0x1F801000u && physical_address < 0x1F804000u)
    {
        return true;
    }

    if (physical_address >= 0xC0000000u)
    {
        return true;
    }

    /* BIOS y regiones no mapeadas  */
    return false;

}

bool memory_write16(Memory *memory, uint32_t address, uint16_t value)
{
    if (address & 1) return false;
    uint32_t physical = normalize_address(address);

    if ((physical & ~1u) == I_STAT_ADDRESS)
    {
        memory->i_stat &= ~((uint32_t)value << ((address & 2) ? 16 : 0));
        return true;
    }

    if ((physical & ~1u) == I_MASK_ADDRESS)
    {
        uint32_t shifted = (uint32_t)value << ((address & 2) ? 16 : 0);
        memory->i_mask = (memory->i_mask & ~(0xFFFFu << ((address & 2) ? 16 : 0))) | shifted;
        return true;
    }

    if ((address & 0x1u) != 0)
    {
        return false;
    }

    uint32_t physical_address = normalize_address(address);

    if (physical_address >= SPU_REGISTERS_START && physical_address < SPU_REGISTERS_END)
    {
        uint32_t offset = physical_address - SPU_REGISTERS_START;
        memory->spu_registers[offset / 2] = value;
        return true;
    }

    if (!memory_write8(memory, address, (uint8_t)(value & 0xFF))) return false;
    if (!memory_write8(memory, address + 1, (uint8_t)(value >> 8))) return false;

    return true;
}

bool memory_read16(Memory *memory, uint32_t address, uint16_t *value)
{
    uint32_t physical = normalize_address(address);

    if ((physical & ~1u) == I_STAT_ADDRESS)
    {
        *value = (address & 1) ? (memory->i_stat >> 16) : (memory->i_stat & 0xFFFF);
        return true;
    }

    if ((physical & ~1u) == I_MASK_ADDRESS)
    {
        *value = (address & 1) ? (memory->i_mask >> 16) : (memory->i_mask & 0xFFFF);
        return true;
    }

    uint8_t lo, hi;

    if (!memory_read8(memory, address,     &lo)) return false;
    if (!memory_read8(memory, address + 1, &hi)) return false;
    *value = (uint16_t)lo | ((uint16_t)hi << 8);
    
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

        case BCC_ADDRESS:
            *value = memory->bcc;
            return true;

        case I_STAT_ADDRESS:
            *value = memory->i_stat;
            return true;

        case I_MASK_ADDRESS:
            *value = memory->i_mask;
            return true;

        case GP0_ADDRESS:
            *value = memory->gpu_read;
            return true;

        case GP1_ADDRESS:
            *value = memory->gpu_stat;
            return true;

        default:
            break;
    }

    for (int t = 0; t < 3; t++)
    {
        uint32_t base = TIMER0_BASE + t * 0x10;
        if (physical_address == base + 0) { *value = memory->timer_counter[t]; return true; }
        if (physical_address == base + 4) { *value = memory->timer_mode[t];    return true; }
        if (physical_address == base + 8) { *value = memory->timer_target[t];  return true; }
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

        case BCC_ADDRESS:
            memory->bcc = value;
            return true;

        case I_STAT_ADDRESS:
            memory->i_stat &= ~value;
            return true;

        case I_MASK_ADDRESS:
            memory->i_mask = value;
            return true;

        case GP0_ADDRESS:
            memory->gp0_command = value;
            /* si es un comando de lectura, preparar gpu_read | por ahora devuelve 0 en cualquier lectura */
            memory->gpu_read = 0;
            return true;

        case GP1_ADDRESS:
            memory->gp1_command = value;

            // GP1 reset (0x00): el BIOS espera que GPUSTAT se resetee
            if ((value & 0xFF000000u) == 0x00000000u)
            {
                memory->gpu_stat = 0x14802000u;
                memory->gpu_read = 0;
            }

            // GP1 display enable (0x03): actualizar bit de display
            if ((value & 0xFF000000u) == 0x03000000u)
            {
                if (value & 0x1)
                {
                    memory->gpu_stat |= 0x00800000u;   /* display enabled */
                }
                else
                {
                    memory->gpu_stat &= ~0x00800000u;
                }
            }

            return true;

        default:
            break;
    }

    for (int t = 0; t < 3; t++)
    {
        uint32_t base = TIMER0_BASE + t * 0x10;
        if (physical_address == base + 0) { memory->timer_counter[t] = 0;      return true; }
        if (physical_address == base + 4) { memory->timer_mode[t] = value;     return true; }
        if (physical_address == base + 8) { memory->timer_target[t] = value;   return true; }
    }

    // valida la palabra entera antes de escribir bytes
    uint32_t offset;

    if (translate_scratchpad_address(address, &offset))
    {
        if (offset > SCRATCHPAD_SIZE - 4)
        {
            return false;
        }
    
        memory->scratchpad[offset] = (uint8_t)(value & 0xFFu);
        memory->scratchpad[offset + 1] = (uint8_t)((value >> 8) & 0xFFu);
        memory->scratchpad[offset + 2] = (uint8_t)((value >> 16) & 0xFFu);
        memory->scratchpad[offset + 3] = (uint8_t)((value >> 24) & 0xFFu);

        return true;
    }

    if (translate_ram_address(address, &offset) && offset <= RAM_SIZE - 4)
    {
        memory->data[offset]     = (uint8_t)(value & 0xFFu);
        memory->data[offset + 1] = (uint8_t)((value >> 8) & 0xFFu);
        memory->data[offset + 2] = (uint8_t)((value >> 16) & 0xFFu);
        memory->data[offset + 3] = (uint8_t)((value >> 24) & 0xFFu);
        return true;
    }

    if (physical_address >= 0xC0000000u)
    {
        return true;
    }

    return false;
}