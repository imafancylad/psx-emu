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
#define OPCODE_LW      0x23
#define OPCODE_LBU     0x24
#define OPCODE_LHU     0x25
#define OPCODE_SB      0x28
#define OPCODE_SH      0x29
#define OPCODE_SW      0x2B

#define FUNCT_SLL  0x00
#define FUNCT_SRL  0x02
#define FUNCT_SRA  0x03
#define FUNCT_JR   0x08
#define FUNCT_SYSCALL 0x0C
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

bool cpu_step(CPU *cpu, Memory *memory)
{
    static uint32_t rb_pc[128];
    static uint32_t rb_instr[128];
    static uint32_t rb_ra[128];
    static uint32_t rb_sp[128];
    static int rb_idx = 0;
    static int dead_zone_dumped = 0;
    static int dumped_eb0 = 0;
    static int dumped_f04 = 0;

    uint32_t instruction;

    if (!memory_read32(memory, cpu->pc, &instruction))
    {
        return false;
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

    /* === DEBUG TEMPORAL: detectar entrada a 0xF00 === */
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
            for (int i = -128; i < 0; i++)
            {
                int j = (rb_idx + i + 128) & 127;
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

    /* Guardar esta instrucción en el ring buffer (antes de ejecutarla) */
    rb_pc[rb_idx]    = cpu->pc;
    rb_instr[rb_idx] = instruction;
    rb_ra[rb_idx]    = cpu->r[31];
    rb_sp[rb_idx]    = cpu->r[29];
    rb_idx = (rb_idx + 1) & 127;

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

    /* Captura estado de delay slot ANTES de resetear */
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
                case FUNCT_JR:
                    cpu->branch_target  = cpu->r[rs];
                    cpu->branch_pending = true;
                    break;
                case FUNCT_SYSCALL:
                    /* TODO: lanzar excepción Syscall */
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
            /* Debug específico del bucle del BIOS */
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
                        fprintf(stderr, "UNKNOWN COP0 CO funct=0x%02X PC=0x%08X\n",
                                (unsigned)(instruction & 0x3F), (unsigned)current_pc);
                        return false;
                    }
                    break;
                default:
                    fprintf(stderr, "UNKNOWN COP0 rs=0x%02X PC=0x%08X\n",
                            (unsigned)rs, (unsigned)current_pc);
                    return false;
            }
            break;

        case OPCODE_LB:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint8_t v;
                if (!memory_read8(memory, addr, &v)) {
                    fprintf(stderr, "LB fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
                cpu->r[rt] = (uint32_t)(int32_t)(int8_t)v;
            }
            break;

        case OPCODE_LBU:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint8_t v;
                if (!memory_read8(memory, addr, &v)) {
                    fprintf(stderr, "LBU fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
                cpu->r[rt] = (uint32_t)v;
            }
            break;

        case OPCODE_LH:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint16_t v;
                if (!memory_read16(memory, addr, &v)) {  /* asegúrate de tener memory_read16 */
                    fprintf(stderr, "LH fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
                cpu->r[rt] = (uint32_t)(int32_t)(int16_t)v;
            }
            break;

        case OPCODE_LHU:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                uint16_t v;
                if (!memory_read16(memory, addr, &v)) {
                    fprintf(stderr, "LHU fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
                cpu->r[rt] = (uint32_t)v;
            }
            break;

        case OPCODE_LW:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                if (!memory_read32(memory, addr, &cpu->r[rt])) {
                    fprintf(stderr, "LW fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SB:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                if (!memory_write8(memory, addr, (uint8_t)cpu->r[rt])) {
                    fprintf(stderr, "SB fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SH:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                if (!memory_write16(memory, addr, (uint16_t)cpu->r[rt])) {
                    fprintf(stderr, "SH fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
            }
            break;

        case OPCODE_SW:
            {
                uint32_t addr = cpu->r[rs] + (uint32_t)simm;
                if (!memory_write32(memory, addr, cpu->r[rt])) {
                    fprintf(stderr, "SW fail PC=0x%08X addr=0x%08X\n",
                            (unsigned)current_pc, (unsigned)addr);
                    return false;
                }
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

    return true;
}