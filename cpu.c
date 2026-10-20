#include "cpu.h"

#include <string.h>
#include <stdio.h>

#define OPCODE_SPECIAL 0x00
#define OPCODE_J       0x02
#define OPCODE_JAL     0x03
#define OPCODE_BEQ     0x04
#define OPCODE_BNE     0x05
#define OPCODE_ANDI    0x0C
#define OPCODE_LUI     0x0F
#define OPCODE_ORI     0x0D
#define OPCODE_ADDI    0x08
#define OPCODE_ADDIU   0x09
#define OPCODE_COP0    0x10
#define OPCODE_LW      0x23
#define OPCODE_SB      0x28
#define OPCODE_SH      0x29
#define OPCODE_SW      0x2B

#define FUNCT_SLL  0x00
#define FUNCT_ADDU 0x21
#define FUNCT_SUBU 0x23
#define FUNCT_AND  0x24
#define FUNCT_OR   0x25
#define FUNCT_SLTU 0x2B

static int32_t sign_extend_16(uint16_t value)
{
    if (value & 0x8000)
    {
        return (int32_t)value - 0x10000;
    }

    return (int32_t)value;
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
    uint32_t instruction;

    if (!memory_read32(memory, cpu->pc, &instruction))
    {
        return false;
    }

    /*
     * Save the previous branch state.
     * The current instruction may be a delay slot.
     */
    bool was_delay_slot = cpu->branch_pending;
    uint32_t delayed_target = cpu->branch_target;

    cpu->branch_pending = false;

    // advance the PC before executing the instruction
    cpu->pc += 4;

    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 0x1F;
    uint32_t rt = (instruction >> 16) & 0x1F;
    uint32_t rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint16_t immediate = (uint16_t)(instruction & 0xFFFF);
    uint32_t funct = instruction & 0x3F;

    if (cpu->pc == 0xBFC00410)
    {
        fprintf(stderr, "DEBUG: instruction=0x%08X opcode=0x%02X rs=%u rt=%u base=0x%08X\n", (unsigned int)instruction, (unsigned int)opcode, (unsigned int)rs, (unsigned int)rt, (unsigned int)cpu->r[rs]);
    }

    switch (opcode)
    {
        case OPCODE_ADDIU:
        {
            int32_t signed_immediate = sign_extend_16(immediate);
            cpu->r[rt] = cpu->r[rs] + (uint32_t)signed_immediate;
            
            break;
        }

        case OPCODE_ADDI:
        {
            int32_t signed_rs = (int32_t)cpu->r[rs];
            int32_t signed_immediate = sign_extend_16(immediate);
            int64_t result = (int64_t)signed_rs + signed_immediate;

            if (result < INT32_MIN || result > INT32_MAX)
            {
                return false;
            }

            cpu->r[rt] = (uint32_t)(int32_t)result;
            break;
        }

        case OPCODE_ANDI:
        {
            cpu->r[rt] = cpu->r[rs] & (instruction & 0xFFFFu);
            break;
        }

        case OPCODE_LUI:
        {
            cpu->r[rt] = (uint32_t)immediate << 16;
            break;
        }

        case OPCODE_ORI:
        {
            cpu->r[rt] = cpu->r[rs] | (instruction & 0xFFFF);
            break;
        }

        case OPCODE_LW:
        {
            int32_t signed_offset = sign_extend_16(immediate);
            uint32_t address = cpu->r[rs] + (uint32_t)signed_offset;

            if (!memory_read32(memory, address, &cpu->r[rt]))
            {
                return false;
            }

            break;
        }
        
        case OPCODE_SH:
        {
            int32_t signed_offset = sign_extend_16(immediate);
            uint32_t address = cpu->r[rs] + (uint32_t)signed_offset;

            if (!memory_write16(memory, address, (uint16_t)cpu->r[rt]))
            {
                fprintf(stderr, "SH failed.\n");
                fprintf(stderr, "PC: 0x%08X\n", (unsigned int)(cpu->pc - 4));
                fprintf(stderr, "Address: 0x%08X\n", (unsigned int)address);
                
                return false;
            }

            break;
        }

        case OPCODE_SB:
        {
            int32_t signed_offset = sign_extend_16(immediate);
            uint32_t address = cpu->r[rs] + (uint32_t)signed_offset;

            if (!memory_write8(memory, address, (uint8_t)cpu->r[rt]))
            {
                fprintf(stderr, "SB failed.\n");
                fprintf(stderr, "PC: 0x%08X\n", (unsigned int)(cpu->pc - 4));
                fprintf(stderr, "Address: 0x%08X\n", (unsigned int)address);
                fprintf(stderr, "Base register (rs=%u): 0x%08X\n", (unsigned int)rs, (unsigned int)cpu->r[rs]);
                fprintf(stderr, "Source register (rt=%u): 0x%08X\n", (unsigned int)rt, (unsigned int)cpu->r[rt]);
                return false;
            }

            break;
        }

        case OPCODE_SW:
        {
            int32_t signed_offset = sign_extend_16(immediate);
            uint32_t address = cpu->r[rs] + (uint32_t)signed_offset;

            if (!memory_write32(memory, address, cpu->r[rt]))
            {
                fprintf(stderr, "SW failed.\n");
                fprintf(stderr, "PC: 0x%08X\n", (unsigned int)(cpu->pc - 4));
                fprintf(stderr, "Address: 0x%08X\n", (unsigned int)address);
                fprintf(stderr, "Value: 0x%08X\n", (unsigned int)cpu->r[rt]);
                fprintf(stderr, "Base register: 0x%08X\n", (unsigned int)cpu->r[rs]);
                return false;
            }

            break;
        }

        case OPCODE_BEQ:
        {
            if (was_delay_slot)
            {
                return false;
            }

            if (cpu->r[rs] == cpu->r[rt])
            {
                int32_t offset = sign_extend_16(immediate);

                cpu->branch_target = cpu->pc + (uint32_t)(offset * 4);
                cpu->branch_pending = true;
            }

            break;
        }

        case OPCODE_BNE:
        {
            if (was_delay_slot)
            {
                return false;
            }

            if (cpu->r[rs] != cpu->r[rt])
            {
                int32_t offset = sign_extend_16(immediate);

                cpu->branch_target = cpu->pc + (uint32_t)(offset * 4);
                cpu->branch_pending = true;
            }

            break;
        }

        case OPCODE_JAL:
        {
            cpu->r[31] = cpu->pc + 4;
            cpu->branch_target = (cpu->pc & 0xF0000000u) | ((instruction & 0x03FFFFFFu) << 2);
            cpu->branch_pending = true;
            break;
        }

        case OPCODE_J:
        {
            if (was_delay_slot)
            {
                return false;
            }

            uint32_t target = (instruction & 0x03FFFFFF) << 2;

            cpu->branch_target = (cpu->pc & 0xF0000000) | target;
            cpu->branch_pending = true;

            break;
        }

        case OPCODE_COP0:
        {
            switch (rs)
            {
                case 0x00:  // MFC0: Move From Coprocessor 0
                    cpu->r[rt] = cpu->cp0[rd];
                    break;

                case 0x04:  // MTC0: Move To Coprocessor 0
                    cpu->cp0[rd] = cpu->r[rt];
                    break;

                default:
                    return false;
            }

            break;
        }

        case OPCODE_SPECIAL:
        {
            switch (funct)
            {
                case FUNCT_SLL:
                    if (rs != 0)
                    {
                        return false;
                    }

                    cpu->r[rd] = cpu->r[rt] << shamt;
                    break;

                case FUNCT_ADDU:
                    cpu->r[rd] = cpu->r[rs] + cpu->r[rt];
                    break;

                case FUNCT_SUBU:
                    cpu->r[rd] = cpu->r[rs] - cpu->r[rt];
                    break;

                case FUNCT_AND:
                    cpu->r[rd] = cpu->r[rs] & cpu->r[rt];
                    break;

                case FUNCT_OR:
                    cpu->r[rd] = cpu->r[rs] | cpu->r[rt];
                    break;

                case FUNCT_SLTU:
                    cpu->r[rd] = (cpu->r[rs] < cpu->r[rt]) ? 1u : 0u;
                    break;

                default:
                    return false;
            }

            break;
        }

        default:
            return false;
    }

    // register zero must always contain zero
    cpu->r[0] = 0;

    // apply the previous branch after executing its delay slot
    if (was_delay_slot)
    {
        cpu->pc = delayed_target;
    }

    return true;
}