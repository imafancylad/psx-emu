#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#define RAM_SIZE (2 * 1024 * 1024)
#define BIOS_SIZE (512 * 1024)

#define BIOS_PHYSICAL_START 0x1FC00000u
#define BIOS_PHYSICAL_END   0x1FC80000u

typedef struct {
    uint8_t data[RAM_SIZE];
    uint8_t bios[BIOS_SIZE];
} Memory;

void memory_init(Memory *memory);

bool memory_load_bios(Memory *memory, const char *path);
bool memory_read8(const Memory *memory, uint32_t address, uint8_t *value);
bool memory_write8(Memory *memory, uint32_t address, uint8_t value);
bool memory_read32(const Memory *memory, uint32_t address, uint32_t *value);
bool memory_write32(Memory *memory, uint32_t address, uint32_t value);

#endif