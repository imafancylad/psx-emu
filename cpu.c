#include "cpu.h"
#include <string.h>
#include <stdio.h>

#define OPCODE_SPECIAL 0x00
#define OPCODE_BCOND   0x01
#define OPCODE_J       0x02
#define OPCODE_JAL     0x03
#define OPCODE_BEQ     0x04
#define OPCODE_BNE     0x05
#define OPCODE_BLEZ    0x06
#define OPCODE_BGTZ    0x07
#define OPCODE_ADDI    0x08
#define OPCODE_ADDIU   0x09
#define OPCODE_SLTI    0x0A
#define OPCODE_SLTIU   0x0B
#define OPCODE_ANDI    0x0C
#define OPCODE_ORI     0x0D
#define OPCODE_XORI    0x0E
#define OPCODE_LUI     0x0F
#define OPCODE_COP0    0x10
#define OPCODE_LB      0x20
#define OPCODE_LH      0x21
#define OPCODE_LWL     0x22
#define OPCODE_LW      0x23
#define OPCODE_LBU     0x24
#define OPCODE_LHU     0x25
#define OPCODE_LWR     0x26
#define OPCODE_SB      0x28
#define OPCODE_SH      0x29
#define OPCODE_SWL     0x2A
#define OPCODE_SW      0x2B
#define OPCODE_SWR     0x2E

#define FUNCT_SLL  0x00
#define FUNCT_SRL  0x02
#define FUNCT_SRA  0x03
#define FUNCT_SLLV 0x04
#define FUNCT_SRLV 0x06
#define FUNCT_SRAV 0x07
#define FUNCT_JR   0x08
#define FUNCT_JALR 0x09
#define FUNCT_SYSCALL 0x0C
#define FUNCT_BREAK 0x0D
#define FUNCT_MFHI 0x10
#define FUNCT_MTHI 0x11
#define FUNCT_MFLO 0x12
#define FUNCT_MTLO 0x13
#define FUNCT_MULT 0x18
#define FUNCT_MULTU 0x19
#define FUNCT_DIV  0x1A
#define FUNCT_DIVU 0x1B
#define FUNCT_ADD  0x20
#define FUNCT_ADDU 0x21
#define FUNCT_SUB  0x22
#define FUNCT_SUBU 0x23
#define FUNCT_AND  0x24
#define FUNCT_OR   0x25
#define FUNCT_XOR  0x26
#define FUNCT_NOR  0x27
#define FUNCT_SLT  0x2A
#define FUNCT_SLTU 0x2B

static int32_t sign_extend_16(uint16_t value)
{
    return (int32_t)(int16_t)value;
}

void cpu_init(CPU *cpu)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->pc = 0;
    cpu->branch_pending = false;
    cpu->branch_target = 0;
}

static void hle_a0(CPU *cpu, Memory *memory, uint32_t syscall_num)
{
    (void)memory;
    fprintf(stderr, "A0 syscall 0x%02X  ra=0x%08X  a0=0x%08X a1=0x%08X a2=0x%08X\n", syscall_num, cpu->r[31], cpu->r[4], cpu->r[5], cpu->r[6]);

    switch (syscall_num)
    {
        case 0x44: /* el bug actual */
            cpu->r[2] = 0;
            break;

        case 0x3F: /* flushcache --> no-op: no tenemos cache */
            cpu->r[2] = 0;
            break;

        case 0x18: /* malloc --> gestion de memoria */
            cpu->r[2] = 0;
            break;

        case 0x49:  /* cdinit */
            cpu->r[2] = 1;   /* :D salio guay */
            break;

        case 0x28:  /* cdsync */
            cpu->r[2] = 0;   /* no hay CD */
            break;
        
        case 0x39:  /* devolver puntero de inicio */
            cpu->r[2] = cpu->r[4];
            break;
        
        case 0x72:  /* ? */
        case 0x1B:  /* ? */
        case 0x2A:  /* ? */
        
        case 0x13:  /* ? */
            cpu->r[2] = 0;
            break;

        default:
            fprintf(stderr, "  -> unhandled, returning 0\n");
            cpu->r[2] = 0;
            break;
    }
}

static void hle_b0(CPU *cpu, Memory *memory, uint32_t syscall_num)
{
    (void)memory;
    fprintf(stderr, "B0 syscall 0x%02X  ra=0x%08X\n", syscall_num, cpu->r[31]);
    
    switch (syscall_num)
    {
        case 0x3D:
            cpu->r[2] = 1;
            break;

        case 0x0C:
            cpu->r[2] = 1;
            break;

        case 0x08:
            cpu->r[2] = 0;
            break;

        case 0x00:
            cpu->r[2] = 0;
            break;

        default:
            cpu->r[2] = 0;
            break;
    }
    
}

static void hle_c0(CPU *cpu, Memory *memory, uint32_t syscall_num)
{
    (void)memory;
    fprintf(stderr, "C0 syscall 0x%02X  ra=0x%08X\n", syscall_num, cpu->r[31]);
    cpu->r[2] = 0;
}

bool cpu_step(CPU *cpu, Memory *memory)
{
    static uint32_t rb_pc[256];
    static uint32_t rb_instr[256];
    static uint32_t rb_ra[256];
    static uint32_t rb_sp[256];
    static int rb_idx = 0;
    static int dead_zone_dumped = 0;
    static int dumped_eb0 = 0;
    static int dumped_f04 = 0;
    static int dumped_6a70 = 0;
    static int dumped_a0 = 0;
    static int dumped_5c4 = 0;
    static int dumped_f80 = 0;
    static int dumped_510f0 = 0;

    uint32_t instruction;

    /* --- interceptar llamadas al sistema de la BIOS --- */
    if (cpu->pc == 0x000000A0 || cpu->pc == 0x800000A0)
    {
        hle_a0(cpu, memory, cpu->r[9]);
        cpu->pc = cpu->r[31];
        return true;
    }

    if (cpu->pc == 0x000000B0 || cpu->pc == 0x800000B0)
    {
        hle_b0(cpu, memory, cpu->r[9]);
        cpu->pc = cpu->r[31];
        return true;
    }

    if (cpu->pc == 0x000000C0 || cpu->pc == 0x800000C0)
    {
        hle_c0(cpu, memory, cpu->r[9]);
        cpu->pc = cpu->r[31];
        return true;
    }

    /* --- FIN HLE --- */

    if (!memory_read32(memory, cpu->pc, &instruction))
    {
        return false;
    }

    if (cpu->pc == 0x800510F0 && !dumped_510f0)
    {
        dumped_510f0 = 1;
        uint32_t ptr = 0;

        memory_read32(memory, 0x800792B0, &ptr);

        uint32_t val = 0;

        if (ptr) memory_read32(memory, ptr, &val);
        fprintf(stderr, "DMA POLL LOOP: *(0x800792B0)=0x%08X, *(ptr)=0x%08X\n", (unsigned)ptr, (unsigned)val);
    }

    if (cpu->pc == 0x00000EB0 && !dumped_eb0)
    {
        dumped_eb0 = 1;
        fprintf(stderr, "\n=== ENTRADA A 0xEB0 ===\n");
        fprintf(stderr, "Memoria en 0xF04-0xF80:\n");

        for (uint32_t a = 0xF04; a < 0xF80; a += 4)
        {
            uint32_t v = 0;

            if (memory_read32(memory, a, &v))
            {
                fprintf(stderr, "  0x%08X: 0x%08X%s\n", a, v, v == 0 ? "  (NOP)" : "");
            }
            else
            {
                fprintf(stderr, "  0x%08X: <read failed>\n", a);
            }
        }

        fprintf(stderr, "=== FIN DUMP 0xEB0 ===\n\n");
    }

    if (cpu->pc == 0x00000F04 && !dumped_f04)
    {
        dumped_f04 = 1;
        fprintf(stderr, "ENTRADA A 0xF04: instr=0x%08X ra=0x%08X\n", (unsigned)instruction, (unsigned)cpu->r[31]);
    }

    if (cpu->pc == 0x00006A70 && !dumped_6a70)
    {
        dumped_6a70 = 1;
        fprintf(stderr, "\n=== ENTRADA A 0x6A70 ===\n");
        fprintf(stderr, "ra=0x%08X sp=0x%08X at=0x%08X v0=0x%08X v1=0x%08X\n", (unsigned)cpu->r[31], (unsigned)cpu->r[29], (unsigned)cpu->r[1], (unsigned)cpu->r[2], (unsigned)cpu->r[3]);
        fprintf(stderr, "Memoria en 0x6A70-0x6B00:\n");

        for (uint32_t a = 0x6A70; a < 0x6B00; a += 4)
        {
            uint32_t v = 0;

            if (memory_read32(memory, a, &v))
            {
                fprintf(stderr, "  0x%08X: 0x%08X%s\n", a, v, v == 0 ? "  (NOP)" : "");
            }
            else
            {
                fprintf(stderr, "  0x%08X: <read failed>\n", a);
            }
        }

        fprintf(stderr, "=== FIN DUMP 0x6A70 ===\n\n");
    }

    if (cpu->pc == 0x000000A0 && !dumped_a0)
    {
        dumped_a0 = 1;
        fprintf(stderr, "\n=== ENTRADA A 0xA0 (handler syscall) ===\n");
        fprintf(stderr, "t1=0x%08X (syscall number) ra=0x%08X sp=0x%08X\n",
                (unsigned)cpu->r[9], (unsigned)cpu->r[31], (unsigned)cpu->r[29]);
        fprintf(stderr, "Memoria en 0xA0-0x180:\n");
        for (uint32_t a = 0xA0; a < 0x180; a += 4)
        {
            uint32_t v = 0;
            if (memory_read32(memory, a, &v))
            {
                fprintf(stderr, "  0x%08X: 0x%08X%s\n", a, v,
                        v == 0 ? "  (NOP)" : "");
            }
            else
            {
                fprintf(stderr, "  0x%08X: <read failed>\n", a);
            }
        }
        fprintf(stderr, "=== FIN DUMP 0xA0 ===\n\n");
    }

    if (cpu->pc == 0x000005C4 && !dumped_5c4)
    {
        dumped_5c4 = 1;
        fprintf(stderr, "\n=== ENTRADA A 0x5C4 (handler real) ===\n");
        fprintf(stderr, "t1=0x%08X (syscall) ra=0x%08X sp=0x%08X\n", (unsigned)cpu->r[9], (unsigned)cpu->r[31], (unsigned)cpu->r[29]);
        fprintf(stderr, "Memoria en 0x5C4-0x6A0:\n");

        for (uint32_t a = 0x5C4; a < 0x6A0; a += 4)
        {
            uint32_t v = 0;

            if (memory_read32(memory, a, &v))
            {
                fprintf(stderr, "  0x%08X: 0x%08X%s\n", a, v, v == 0 ? "  (NOP)" : "");
            }
        }

        fprintf(stderr, "=== FIN DUMP 0x5C4 ===\n\n");

        uint32_t handler = 0;

        if (memory_read32(memory, 0x310, &handler))
        {
            fprintf(stderr, "  *(0x310) = 0x%08X (handler syscall 0x44)\n", (unsigned)handler);
        }

        fprintf(stderr, "Tabla 0x200-0x400:\n");

        for (uint32_t a = 0x200; a < 0x400; a += 4)
        {
            uint32_t v = 0;

            if (memory_read32(memory, a, &v) && v != 0)
            {
                fprintf(stderr, "  0x%03X: 0x%08X\n", a, v);
            }
        }
    }

    if (cpu->pc == 0x00001000)
    {
        static int dumped_1000 = 0;

        if (!dumped_1000)
        {
            dumped_1000 = 1;
            uint32_t gp_plus_80 = 0;
            memory_read32(memory, cpu->r[28] + 0x80, &gp_plus_80);
            fprintf(stderr, "ENTRADA A 0x1000: k0 se cargará con 0x%08X (gp+0x80)\n", (unsigned)gp_plus_80);
        }
    }

    if (cpu->pc >= 0x00000F80 && cpu->pc < 0x00001000 && !dumped_f80)
    {
        dumped_f80 = 1;
        fprintf(stderr, "\n=== ENTRADA A 0xF80 (zona de stubs) ===\n");
        fprintf(stderr, "ra=0x%08X sp=0x%08X t1=0x%08X\n", (unsigned)cpu->r[31], (unsigned)cpu->r[29], (unsigned)cpu->r[9]);
        fprintf(stderr, "Memoria en 0xF80-0x1020:\n");

        for (uint32_t a = 0xF80; a < 0x1020; a += 4)
        {
            uint32_t v = 0;

            if (memory_read32(memory, a, &v))
            {
                fprintf(stderr, "  0x%08X: 0x%08X%s\n", a, v, v == 0 ? "  (NOP)" : "");
            }
        }

        fprintf(stderr, "Ultimas 256 instrucciones:\n");

        for (int i = -256; i < 0; i++)
        {
            int j = (rb_idx + i + 256) & 255;
            if (rb_pc[j] == 0 && rb_instr[j] == 0) continue;
            fprintf(stderr, "  [%+04d] PC=0x%08X instr=0x%08X ra=0x%08X\n", i, (unsigned)rb_pc[j], (unsigned)rb_instr[j], (unsigned)rb_ra[j]);
        }

        fprintf(stderr, "=== FIN DUMP 0xF80 ===\n\n");
    }

    /* ---- DEBUG TEMPORAL: detectar entrada a 0xF00 ---- */
    if (cpu->pc >= 0x00000F00 && cpu->pc < 0x00001000)
    {
        if (!dead_zone_dumped)
        {
            dead_zone_dumped = 1;
            fprintf(stderr, "=== ENTRADA A ZONA MUERTA (0xF00) ===\n");
            fprintf(stderr, "Primera PC en zona: 0x%08X instr=0x%08X\n",
                    (unsigned)cpu->pc, (unsigned)instruction);
            fprintf(stderr, "Registros: ra=0x%08X sp=0x%08X gp=0x%08X\n",
                    (unsigned)cpu->r[31], (unsigned)cpu->r[29],
                    (unsigned)cpu->r[28]);
            fprintf(stderr, "  v0=0x%08X v1=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X\n",
                    (unsigned)cpu->r[2], (unsigned)cpu->r[3],
                    (unsigned)cpu->r[4], (unsigned)cpu->r[5],
                    (unsigned)cpu->r[6], (unsigned)cpu->r[7]);
            fprintf(stderr, "  t0=0x%08X t1=0x%08X t2=0x%08X t3=0x%08X\n",
                    (unsigned)cpu->r[8], (unsigned)cpu->r[9],
                    (unsigned)cpu->r[10], (unsigned)cpu->r[11]);
            fprintf(stderr, "  s0=0x%08X s1=0x%08X s2=0x%08X s3=0x%08X\n",
                    (unsigned)cpu->r[16], (unsigned)cpu->r[17],
                    (unsigned)cpu->r[18], (unsigned)cpu->r[19]);
            fprintf(stderr, "Ultimas 32 instrucciones ejecutadas:\n");
            
            fprintf(stderr, "Ultimas 128 instrucciones (mas recientes al final):\n");
            for (int i = -256; i < 0; i++)
            {
                int j = (rb_idx + i + 256) & 255;
                if (rb_pc[j] == 0 && rb_instr[j] == 0)
                    continue;
                fprintf(stderr,
                    "  [%+04d] PC=0x%08X instr=0x%08X ra=0x%08X sp=0x%08X\n",
                    i, (unsigned)rb_pc[j], (unsigned)rb_instr[j],
                    (unsigned)rb_ra[j], (unsigned)rb_sp[j]);
            }

            fprintf(stderr, "=== FIN ZONA MUERTA ===\n");
        }
    }

    /* Guardar esta instruccion en el buffer (antes de ejecutarla) */
    rb_pc[rb_idx]    = cpu->pc;
    rb_instr[rb_idx] = instruction;
    rb_ra[rb_idx]    = cpu->r[31];
    rb_sp[rb_idx]    = cpu->r[29];
    rb_idx = (rb_idx + 1) & 255;

    /* --- Debug puntual del bucle del BIOS --- */
    if (cpu->pc >= 0x000005B4 && cpu->pc <= 0x000005B8)
    {
        static unsigned int dbg = 0;
        if (dbg < 20)
        {
            fprintf(stderr,
                "TRACE PC=0x%08X instr=0x%08X v0=0x%08X v1=0x%08X at=0x%08X bp=%d\n",
                (unsigned)cpu->pc, (unsigned)instruction,
                (unsigned)cpu->r[2], (unsigned)cpu->r[3],
                (unsigned)cpu->r[1], cpu->branch_pending);
            dbg++;
        }
    }

    /* captura estado de delay slot antes del reset */
    bool was_delay_slot = cpu->branch_pending;
    uint32_t delayed_target = cpu->branch_target;

    cpu->branch_pending = false;
    cpu->branch_target = 0;

    uint32_t current_pc = cpu->pc;
    cpu->pc += 4;

    uint32_t opcode    = instruction >> 26;
    uint32_t rs        = (instruction >> 21) & 0x1F;
    uint32_t rt        = (instruction >> 16) & 0x1F;
    uint32_t rd        = (instruction >> 11) & 0x1F;
    uint32_t shamt     = (instruction >> 6) & 0x1F;
    uint16_t immediate = (uint16_t)(instruction & 0xFFFF);
    uint32_t funct     = instruction & 0x3F;
    int32_t  simm      = sign_extend_16(immediate);

    switch (opcode)
    {
        case OPCODE_SPECIAL:
            switch (funct)
            {
                case FUNCT_SLL:  cpu->r[rd] = cpu->r[rt] << shamt; break;
                case FUNCT_SRL:  cpu->r[rd] = cpu->r[rt] >> shamt; break;
                case FUNCT_SRA:  cpu->r[rd] = (uint32_t)((int32_t)cpu->r[rt] >> shamt); break;
                case FUNCT_SLLV: cpu->r[rd] = cpu->r[rt] << (cpu->r[rs] & 0x1F); break;
                case FUNCT_SRLV: cpu->r[rd] = cpu->r[rt] >> (cpu->r[rs] & 0x1F); break;
                case FUNCT_SRAV: cpu->r[rd] = (uint32_t)((int32_t)cpu->r[rt] >> (cpu->r[rs] & 0x1F)); break;
                
                case FUNCT_JR:
                    cpu->branch_target  = cpu->r[rs];
                    cpu->branch_pending = true;
                    break;

                case FUNCT_JALR:
                    cpu->r[rd]          = cpu->pc + 4;   /* pc ya apunta al delay slot */
                    cpu->branch_target  = cpu->r[rs];
                    cpu->branch_pending = true;
                    break;

                case FUNCT_SYSCALL:
                    /* lanzar excepción Syscall */
                    break;

                case FUNCT_BREAK:
                    /* lanzar excepcion Break */
                    break;

                case FUNCT_ADD:  cpu->r[rd] = cpu->r[rs] + cpu->r[rt]; break; /* wrap */
                case FUNCT_ADDU: cpu->r[rd] = cpu->r[rs] + cpu->r[rt]; break;
                case FUNCT_SUB:  cpu->r[rd] = cpu->r[rs] - cpu->r[rt]; break; /* wrap */
                case FUNCT_SUBU: cpu->r[rd] = cpu->r[rs] - cpu->r[rt]; break;
                case FUNCT_AND:  cpu->r[rd] = cpu->r[rs] & cpu->r[rt]; break;
                case FUNCT_OR:   cpu->r[rd] = cpu->r[rs] | cpu->r[rt]; break;
                case FUNCT_XOR:  cpu->r[rd] = cpu->r[rs] ^ cpu->r[rt]; break;
                case FUNCT_NOR:  cpu->r[rd] = ~(cpu->r[rs] | cpu->r[rt]); break;
                case FUNCT_SLT:
                    cpu->r[rd] = ((int32_t)cpu->r[rs] < (int32_t)cpu->r[rt]) ? 1u : 0u;
                    break;
                case FUNCT_SLTU:
                    cpu->r[rd] = (cpu->r[rs] < cpu->r[rt]) ? 1u : 0u;
                    break;

                case FUNCT_MFHI: cpu->r[rd] = cpu->hi; break;
                case FUNCT_MTHI: cpu->hi    = cpu->r[rs]; break;
                case FUNCT_MFLO: cpu->r[rd] = cpu->lo; break;
                case FUNCT_MTLO: cpu->lo    = cpu->r[rs]; break;

                case FUNCT_MULT:
                {
                    int64_t p = (int64_t)(int32_t)cpu->r[rs] * (int32_t)cpu->r[rt];
                    cpu->lo = (uint32_t)(p & 0xFFFFFFFFu);
                    cpu->hi = (uint32_t)(uint64_t)(p >> 32);
                    break;
                }

                case FUNCT_MULTU:
                {
                    uint64_t p = (uint64_t)cpu->r[rs] * cpu->r[rt];
                    cpu->lo = (uint32_t)(p & 0xFFFFFFFFu);
                    cpu->hi = (uint32_t)(p >> 32);
                    break;
                }

                case FUNCT_DIV:
                    if (cpu->r[rt] != 0)
                    {
                        cpu->lo = (uint32_t)((int32_t)cpu->r[rs] / (int32_t)cpu->r[rt]);
                        cpu->hi = (uint32_t)((int32_t)cpu->r[rs] % (int32_t)cpu->r[rt]);
                    }
                    break;

                case FUNCT_DIVU:
                    if (cpu->r[rt] != 0)
                    {
                        cpu->lo = cpu->r[rs] / cpu->r[rt];
                        cpu->hi = cpu->r[rs] % cpu->r[rt];
                    }
                break;

                default:
                    fprintf(stderr, "UNKNOWN funct=0x%02X instr=0x%08X PC=0x%08X\n",
                            (unsigned)funct, (unsigned)instruction, (unsigned)current_pc);
                    return false;
            }
            break;

        case OPCODE_BCOND:
            {
                uint32_t cond = (instruction >> 16) & 0x1F;
                bool take = false;
                bool link = false;
                switch (cond)
                {
                    case 0x00: take = ((int32_t)cpu->r[rs] <  0); break; /* BLTZ   */
                    case 0x01: take = ((int32_t)cpu->r[rs] >= 0); break; /* BGEZ   */
                    case 0x10: take = ((int32_t)cpu->r[rs] <  0); link = true; break; /* BLTZAL */
                    case 0x11: take = ((int32_t)cpu->r[rs] >= 0); link = true; break; /* BGEZAL */
                    default:
                        fprintf(stderr, "UNKNOWN BCOND rt=0x%02X PC=0x%08X\n",
                                (unsigned)cond, (unsigned)current_pc);
                        return false;
                }
                if (link) cpu->r[31] = cpu->pc + 4;
                if (take)
                {
                    cpu->branch_target  = cpu->pc + (uint32_t)(simm * 4);
                    cpu->branch_pending = true;
                }
            }
            break;

        case OPCODE_J:
            cpu->branch_target  = (cpu->pc & 0xF0000000u) | ((instruction & 0x03FFFFFFu) << 2);
            cpu->branch_pending = true;
            break;

        case OPCODE_JAL:
            cpu->r[31]          = cpu->pc + 4;
            cpu->branch_target  = (cpu->pc & 0xF0000000u) | ((instruction & 0x03FFFFFFu) << 2);
            cpu->branch_pending = true;
            break;

        case OPCODE_BEQ:
            if (cpu->r[rs] == cpu->r[rt])
            {
                cpu->branch_target  = cpu->pc + (uint32_t)(simm * 4);
                cpu->branch_pending = true;
            }
            break;

        case OPCODE_BNE:
            if (cpu->r[rs] != cpu->r[rt])
            {
                cpu->branch_target  = cpu->pc + (uint32_t)(simm * 4);
                cpu->branch_pending = true;
            }
            /* Depuracion especifica del bucle del BIOS */
            if (current_pc == 0x000005B4)
            {
                fprintf(stderr,
                    "BNE@0x5B4: r%u=0x%08X vs r%u=0x%08X -> %s tgt=0x%08X\n",
                    (unsigned)rs, (unsigned)cpu->r[rs],
                    (unsigned)rt, (unsigned)cpu->r[rt],
                    (cpu->r[rs] != cpu->r[rt]) ? "TAKEN" : "not",
                    (unsigned)cpu->branch_target);
            }
            break;

        case OPCODE_BLEZ:
            if ((int32_t)cpu->r[rs] <= 0)
            {
                cpu->branch_target  = cpu->pc + (uint32_t)(simm * 4);
                cpu->branch_pending = true;
            }
            break;

        case OPCODE_BGTZ:
            if ((int32_t)cpu->r[rs] > 0)
            {
                cpu->branch_target  = cpu->pc + (uint32_t)(simm * 4);
                cpu->branch_pending = true;
            }
            break;

        case OPCODE_ADDI:
            cpu->r[rt] = cpu->r[rs] + (uint32_t)simm; /* wrap (ideal: excepción) */
            break;

        case OPCODE_ADDIU:
            cpu->r[rt] = cpu->r[rs] + (uint32_t)simm;
            break;

        case OPCODE_SLTI:
            cpu->r[rt] = ((int32_t)cpu->r[rs] < simm) ? 1u : 0u;
            break;

        case OPCODE_SLTIU:
            cpu->r[rt] = (cpu->r[rs] < (uint32_t)simm) ? 1u : 0u;
            break;

        case OPCODE_ANDI:
            cpu->r[rt] = cpu->r[rs] & (uint32_t)immediate;
            break;

        case OPCODE_ORI:
            cpu->r[rt] = cpu->r[rs] | (uint32_t)immediate;
            break;

        case OPCODE_XORI:
            cpu->r[rt] = cpu->r[rs] ^ (uint32_t)immediate;
            break;

        case OPCODE_LUI:
            cpu->r[rt] = (uint32_t)immediate << 16;
            break;

        case OPCODE_COP0:
            switch (rs)
            {
                case 0x00: cpu->r[rt] = cpu->cp0[rd]; break;  /* MFC0 */
                case 0x04: cpu->cp0[rd] = cpu->r[rt]; break;  /* MTC0 */
                case 0x10:
                    if ((instruction & 0x3F) == 0x10)  /* RFE */
                    {
                        uint32_t s = cpu->cp0[12];
                        cpu->cp0[12] = (s & ~0xFu) | ((s >> 2) & 0xFu);
                    }
                    else
                    {
                        fprintf(stderr, "UNKNOWN COP0 CO funct=0x%02X PC=0x%08X\n", (unsigned)(instruction & 0x3F), (unsigned)current_pc);
                        return false;
                    }
                    break;

                default:
                    fprintf(stderr, "UNKNOWN COP0 rs=0x%02X PC=0x%08X\n", (unsigned)rs, (unsigned)current_pc);
                    return false;
            }
            break;

        case OPCODE_LB:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint8_t v;

                if (!memory_read8(memory, addr, &v))
                {
                    fprintf(stderr, "LB fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }

                cpu->r[rt] = (uint32_t)(int32_t)(int8_t)v;
            }
            break;

        case OPCODE_LBU:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint8_t v;

                if (!memory_read8(memory, addr, &v))
                {
                    fprintf(stderr, "LBU fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }

                cpu->r[rt] = (uint32_t)v;
            }
            break;

        case OPCODE_LH:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint16_t v;

                if (!memory_read16(memory, addr, &v))
                {  /* asegurarse de tener memory_read16 */
                    fprintf(stderr, "LH fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
                cpu->r[rt] = (uint32_t)(int32_t)(int16_t)v;
            }
            break;

        case OPCODE_LHU:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint16_t v;

                if (!memory_read16(memory, addr, &v))
                {
                    fprintf(stderr, "LHU fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }

                cpu->r[rt] = (uint32_t)v;
            }
            break;

        case OPCODE_LW:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;

                if (!memory_read32(memory, addr, &cpu->r[rt]))
                {
                    fprintf(stderr, "LW fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SB:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;

                if (!memory_write8(memory, addr, (uint8_t)cpu->r[rt]))
                {
                    fprintf(stderr, "SB fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SH:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;

                if (!memory_write16(memory, addr, (uint16_t)cpu->r[rt]))
                {
                    fprintf(stderr, "SH fail PC=0x%08X addr=0x%08X\n", (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SW:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;

                if ((addr >= 0x00000F80 && addr < 0x00001000) || (addr >= 0x80000F80 && addr < 0x80001000))
                {
                    fprintf(stderr, "SW a STUB ZONE: PC=0x%08X addr=0x%08X value=0x%08X\n", (unsigned)current_pc, (unsigned)addr, (unsigned)cpu->r[rt]);
                }

                if ((addr >= 0x00000F80 && addr < 0x00001000) || (addr >= 0x80000F80 && addr < 0x80001000))
                {
                    fprintf(stderr, "SW a STUB ZONE: PC=0x%08X addr=0x%08X value=0x%08X\n", (unsigned)current_pc, (unsigned)addr, (unsigned)cpu->r[rt]);
                }

                if (!memory_write32(memory, addr, cpu->r[rt])) {
                    fprintf(stderr, "SW fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_LWL:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint32_t aligned = addr & ~3u;
                uint32_t word = 0;
                if (!memory_read32(memory, aligned, &word)) return false;
                uint32_t shift = (addr & 3u) * 8;
                uint32_t mask = 0xFFFFFFFFu << shift;
                cpu->r[rt] = (cpu->r[rt] & ~mask) | ((word << shift) & mask);
            }
            break;

        case OPCODE_LWR:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint32_t aligned = addr & ~3u;
                uint32_t word = 0;
                if (!memory_read32(memory, aligned, &word)) return false;
                uint32_t shift = (addr & 3u) * 8;
                uint32_t mask = 0xFFFFFFFFu >> (24 - shift);
                cpu->r[rt] = (cpu->r[rt] & ~mask) | (word >> (24 - shift));
            }
            break;

        case OPCODE_SWL:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint32_t aligned = addr & ~3u;
                uint32_t word = 0;
                if (!memory_read32(memory, aligned, &word)) return false;
                uint32_t shift = (addr & 3u) * 8;
                uint32_t mask = 0xFFFFFFFFu << shift;
                word = (word & ~mask) | ((cpu->r[rt] << shift) & mask);
                if (!memory_write32(memory, aligned, word)) return false;
            }
            break;

        case OPCODE_SWR:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint32_t aligned = addr & ~3u;
                uint32_t word = 0;
                if (!memory_read32(memory, aligned, &word)) return false;
                uint32_t shift = (addr & 3u) * 8;
                uint32_t mask = 0xFFFFFFFFu >> (24 - shift);
                word = (word & ~mask) | (cpu->r[rt] >> (24 - shift));
                if (!memory_write32(memory, aligned, word)) return false;
            }
            break;

        default:
            fprintf(stderr, "UNKNOWN opcode=0x%02X instr=0x%08X PC=0x%08X\n",
                    (unsigned)opcode, (unsigned)instruction, (unsigned)current_pc);
            return false;
    }

    cpu->r[0] = 0;

        /* === DEBUG TEMPORAL: quién salta a 0xF00 === */
    if (cpu->branch_pending &&
        cpu->branch_target >= 0x00000F00 &&
        cpu->branch_target < 0x00001000)
    {
        fprintf(stderr,
            "SALTO A ZONA MUERTA: desde PC=0x%08X instr=0x%08X target=0x%08X\n"
            "  rs=%u(0x%08X) rt=%u(0x%08X) rd=%u(0x%08X) ra=0x%08X\n",
            (unsigned)current_pc, (unsigned)instruction,
            (unsigned)cpu->branch_target,
            (unsigned)rs, (unsigned)cpu->r[rs],
            (unsigned)rt, (unsigned)cpu->r[rt],
            (unsigned)rd, (unsigned)cpu->r[rd],
            (unsigned)cpu->r[31]);
    }

    /* --- Resolución de PC (delay slot) --- */
    if (was_delay_slot)
    {
        if (cpu->branch_pending)
        {
            /* El delay slot era a su vez un branch: gana su target,
               y no dejamos branch pendiente (su delay slot queda saltado). */
            cpu->pc             = cpu->branch_target;
            cpu->branch_pending = false;
            cpu->branch_target  = 0;
        }
        else
        {
            /* Caso normal: saltamos al target del branch original */
            cpu->pc = delayed_target;
        }
    }
    /* si no era delay slot, cpu->pc ya quedó en current_pc+4 */

    uint32_t i_stat = 0, i_mask = 0;
    memory_read32(memory, I_STAT_ADDRESS, &i_stat);
    memory_read32(memory, I_MASK_ADDRESS, &i_mask);

    uint32_t pending = i_stat & i_mask;
    uint32_t sr = cpu->cp0[12];
    bool iec = (sr & 0x1) != 0;                                     // IEc: current interrupt enable
    bool im = (sr & 0x400) != 0;                                    // IM: interrupt mask global

    if (pending && iec && im)
    {
        cpu->cp0[14] = current_pc;                                  // EPC
        cpu->cp0[13] = (cpu->cp0[13] & ~0x7C) | 0x400;              // IP2 (bit 10)
        cpu->cp0[12] = (sr & ~0x3Fu) | 0x2;                         // EXL=1, KUc=0
        uint32_t vector = (sr & 0x00400000) ? 0xBFC00180u : 0x80000080u;
        cpu->pc = vector;
    }

    return true;
}