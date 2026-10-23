#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"

#define CPU_REGISTER_COUNT 32
#define CP0_REGISTER_COUNT 32

typedef struct {
    uint32_t r[CPU_REGISTER_COUNT];
    uint32_t cp0[CP0_REGISTER_COUNT];
    uint32_t pc;
    uint32_t hi;
    uint32_t lo;

    bool branch_pending;
    uint32_t branch_target;
} CPU;

void cpu_init(CPU *cpu);
bool cpu_step(CPU *cpu, Memory *memory);

#endif