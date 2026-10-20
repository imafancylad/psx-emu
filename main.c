#include <stdio.h>

#include "cpu.h"
#include "memory.h"

#define MAX_STEPS 100

int main(int argc, char *argv[])
{
    CPU cpu;
    Memory memory;
    memory_init(&memory);
    cpu_init(&cpu);

    bool bios_loaded = false;
    uint32_t bios_first_instruction = 0;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [bios_file]\n", argv[0]);
        return 1;
    }

    if (argc == 2)
    {
        if (!memory_load_bios(&memory, argv[1]))
        {
            fprintf(stderr, "Failed to load BIOS file.\n");
            fprintf(stderr, "Expected a valid 512 KiB BIOS image.\n");
            return 1;
        }

        if (!memory_read32(&memory, 0xBFC00000, &bios_first_instruction))
        {
            fprintf(stderr, "Failed to read BIOS entry point.\n");
            return 1;
        }

        bios_loaded = true;
    }

    const uint32_t program[] =
    {
        0x24080005,  /* addiu $t0, $zero, 5 */
        0x24090005,  /* addiu $t1, $zero, 5 */
        0x11090002,  /* beq   $t0, $t1, target_beq */
        0x240A0063,  /* addiu $t2, $zero, 99: delay slot */
        0x2409006F,  /* addiu $t1, $zero, 111: skipped */
        0x240B0007,  /* target_beq: addiu $t3, $zero, 7 */

        0x15090002,  /* bne   $t0, $t1, target_bne */
        0x240C002A,  /* addiu $t4, $zero, 42: delay slot */
        0x240D0008,  /* addiu $t5, $zero, 8 */
        0x240E000B,  /* target_bne: addiu $t6, $zero, 11 */

        0x0800000D,  /* j target_jump */
        0x240F0021,  /* addiu $t7, $zero, 33: delay slot */
        0x2410002C,  /* addiu $s0, $zero, 44: skipped */
        0x24110037   /* target_jump: addiu $s1, $zero, 55 */
    };

    const uint32_t instruction_count = sizeof(program) / sizeof(program[0]);
    const uint32_t program_size = (uint32_t)sizeof(program);

    for (uint32_t i = 0; i < instruction_count; i++)
    {
        if (!memory_write32(&memory, i * 4, program[i]))
        {
            fprintf(stderr, "Failed to load program.\n");
            return 1;
        }
    }

    uint32_t steps = 0;

    while (cpu.pc < program_size && steps < MAX_STEPS)
    {
        if (!cpu_step(&cpu, &memory))
        {
            fprintf(stderr, "Instruction execution failed.\n");
            fprintf(stderr, "PC: 0x%08X\n", (unsigned int)cpu.pc);
            return 1;
        }

        steps++;
    }

    uint32_t value_kseg0 = 0;
    uint32_t value_kseg1 = 0;
    uint32_t value_mirror = 0;

    bool memory_alias_test =
        memory_write32(&memory, 0x00000100, 0xDEADBEEF) &&
        memory_read32(&memory, 0x80000100, &value_kseg0) &&
        memory_read32(&memory, 0xA0000100, &value_kseg1) &&
        memory_read32(&memory, 0x00200100, &value_mirror);

    printf("KSEG0 read: 0x%08X\n", (unsigned int)value_kseg0);
    printf("KSEG1 read: 0x%08X\n", (unsigned int)value_kseg1);
    printf("RAM mirror read: 0x%08X\n", (unsigned int)value_mirror);

    printf("PSX emulator\n");

    if (bios_loaded)
    {
        printf("BIOS loaded successfully.\n");
        printf("BIOS entry instruction: 0x%08X\n", (unsigned int)bios_first_instruction);
    }

    printf("Register t0: %u\n", (unsigned int)cpu.r[8]);
    printf("Register t1: %u\n", (unsigned int)cpu.r[9]);
    printf("Register t2: %u\n", (unsigned int)cpu.r[10]);
    printf("Register t3: %u\n", (unsigned int)cpu.r[11]);
    printf("Register t4: %u\n", (unsigned int)cpu.r[12]);
    printf("Register t5: %u\n", (unsigned int)cpu.r[13]);
    printf("Register t6: %u\n", (unsigned int)cpu.r[14]);
    printf("Register t7: %u\n", (unsigned int)cpu.r[15]);
    printf("Register s0: %u\n", (unsigned int)cpu.r[16]);
    printf("Register s1: %u\n", (unsigned int)cpu.r[17]);
    printf("Final PC: 0x%08X\n", (unsigned int)cpu.pc);
    printf("Instructions executed: %u\n", (unsigned int)steps);

    if (cpu.r[8] != 5 ||
        cpu.r[9] != 5 ||
        cpu.r[10] != 99 ||
        cpu.r[11] != 7 ||
        cpu.r[12] != 42 ||
        cpu.r[13] != 8 ||
        cpu.r[14] != 11 ||
        cpu.r[15] != 33 ||
        cpu.r[16] != 0 ||
        cpu.r[17] != 55 ||
        cpu.pc != program_size ||
        steps != 12 ||
        cpu.branch_pending ||
        !memory_alias_test)
    {
        fprintf(stderr, "CPU test failed.\n");
        return 1;
    }

    printf("CPU test passed.\n");

    return 0;
}