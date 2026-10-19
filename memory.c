#include "memory.h"

#include <string.h>

void memory_init(Memory *memory)
{
    memset(memory->data, 0, sizeof(memory->data));
}

bool memory_read8(const Memory *memory, uint32_t address, uint8_t *value)
{
    if (address >= RAM_SIZE)
    {
        return false;
    }

    *value = memory->data[address];

    return true;
}

bool memory_write8(Memory *memory, uint32_t address, uint8_t value)
{
    if (address >= RAM_SIZE)
    {
        return false;
    }

    memory->data[address] = value;

    return true;
}

bool memory_read32(const Memory *memory, uint32_t address, uint32_t *value)
{
    if (address > RAM_SIZE - 4 || address % 4 != 0)
    {
        return false;
    }

    *value = (uint32_t)memory->data[address] | ((uint32_t)memory->data[address + 1] << 8) | ((uint32_t)memory->data[address + 2] << 16) | ((uint32_t)memory->data[address + 3] << 24);
    return true;
}

bool memory_write32(Memory *memory, uint32_t address, uint32_t value)
{
    if (address > RAM_SIZE - 4 || address % 4 != 0)
    {
        return false;
    }

    memory->data[address] = (uint8_t)(value & 0xFF);
    memory->data[address + 1] = (uint8_t)((value >> 8) & 0xFF);
    memory->data[address + 2] = (uint8_t)((value >> 16) & 0xFF);
    memory->data[address + 3] = (uint8_t)((value >> 24) & 0xFF);

    return true;
}