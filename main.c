#include <stdint.h>
#include <stdio.h>

#include "memory.h"

#define NUM_REGISTERS 32

typedef struct {
    uint32_t r[NUM_REGISTERS];
    uint32_t pc;
} CPU;

int main(void)
{
    CPU cpu = {0};
    Memory memory;

    memory_init(&memory);

    cpu.pc = 0xBFC00000;

    printf("PSX emulator\n");
    printf("Initial PC: 0x%08X\n", (unsigned int)cpu.pc);
    printf("Register zero: %u\n", (unsigned int)cpu.r[0]);

    if (!memory_write32(&memory, 0, 0x12345678))
    {
        fprintf(stderr, "Failed to write to memory.\n");
        return 1;
    }

    uint32_t value = 0;

    if (!memory_read32(&memory, 0, &value))
    {
        fprintf(stderr, "Failed to read from memory.\n");
        return 1;
    }

    printf("Memory value: 0x%08X\n", (unsigned int)value);

    return 0;
}