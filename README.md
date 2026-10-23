# PSX Emulator

A PlayStation 1 emulator written from scratch in C.

> **Status: Work in progress.** The emulator has progressed through a significant portion of the PSX BIOS startup sequence and can reach what appears to be the BIOS idle loop. Several CPU instructions and memory-mapped hardware registers have been implemented to get this far.
>
> The next development milestone is loading and executing PS-EXE files. However, reaching the BIOS idle loop does not mean that the emulator is fully compatible or that all hardware components are correctly implemented. GPU rendering, accurate interrupts, DMA transfers, and other hardware features still require development and testing.

The goal is to build the emulator incrementally, implementing and testing CPU instructions, memory, and hardware components as the project progresses. Compatibility will depend on the features that are implemented and validated; full compatibility is not guaranteed.

## Current Status

### BIOS Execution

The emulator has progressed beyond multiple previously encountered BIOS execution blockers.

The following issues have been addressed during development:

| # | Approximate Steps | Component | Status |
|---|---:|---|---|
| 1 | 79K | I_MASK | Resolved |
| 2 | 2.6M | JALR | Resolved |
| 3 | 2.68M | SLLV | Resolved |
| 4 | 12M | VBlank loop 1 | Resolved |
| 5 | 19M | GP0 | Resolved |
| 6 | 19.16M | DICR | Resolved |
| 7 | 50M | DMA6_CHCR | Resolved |
| 8 | 50M | DMA2_CHCR | Resolved |
| 9 | 50M | VBlank counter 1 | Resolved |
| 10 | 50M | GPUSTAT bit 27 | Resolved |
| 11 | 50M | VBlank counter 2 | Resolved |
| 12 | 50M | BIOS idle loop / events | Under investigation |

These results indicate that BIOS execution has progressed significantly. The idle loop may be a normal waiting state in which the BIOS expects hardware events or application work.

Further testing is needed to establish which BIOS services and hardware behaviors work correctly.

### Next Milestone: PS-EXE Loading

The next major objective is to load and execute PlayStation executable files.

The planned loader will:

- Read and validate the PS-EXE header.
- Extract the executable entry point, load address, and payload size.
- Copy the executable payload into emulated RAM.
- Initialize the CPU program counter and relevant registers.
- Begin execution at the executable entry point.
- Report unsupported instructions or missing hardware behavior encountered during execution.

The intended command-line interface is:

    ./psx-emu game.iso

This interface is a development target and should be considered implemented only after it has been added and tested.

Loading a PS-EXE directly is different from booting a commercial game from the CD-ROM through the original BIOS startup sequence.

## Current Features

### Memory

The memory subsystem has been developed to support BIOS execution and basic hardware access.

Implemented or partially implemented functionality includes:

- 2 MiB of RAM.
- 512 KiB BIOS image support.
- BIOS loading from a user-provided file.
- BIOS image size validation.
- Byte reads and RAM byte writes.
- 32-bit word reads and writes.
- Alignment checks for 32-bit memory accesses.
- Little-endian memory access.
- Basic address translation for KSEG0 and KSEG1.
- Basic RAM mirroring within the supported physical address region.
- BIOS mapping at physical addresses `0x1FC00000`–`0x1FC7FFFF`.
- Basic protection against writes to the BIOS and accesses to unmapped regions.
- Memory-mapped I/O handling for selected PSX hardware registers.
- DMA register access required to progress through the observed BIOS initialization sequence.

The memory subsystem is still incomplete. Accurate hardware register behavior, memory access edge cases, and interactions between hardware components require further testing.

### CPU

The CPU implementation targets the MIPS R3000A architecture used by the original PlayStation.

The project has progressed beyond the initial synthetic CPU test and now includes additional instruction handling needed for BIOS execution.

Implemented or partially implemented functionality includes:

- 32 general-purpose registers.
- Program counter (`PC`).
- Register zero (`$zero`) maintained as zero.
- Single-step instruction execution through `cpu_step`.
- Branch and jump handling.
- Branch delay-slot handling.
- Additional MIPS instructions implemented as required by BIOS execution.

Instructions implemented during the earlier development stage include:

| Instruction | Description |
|---|---|
| `ADDU` | Add unsigned without an arithmetic overflow exception |
| `SUBU` | Subtract unsigned without an arithmetic overflow exception |
| `AND` | Bitwise AND |
| `OR` | Bitwise OR |
| `ADDIU` | Add immediate without an arithmetic overflow exception |
| `LUI` | Load upper immediate |
| `LW` | Load 32-bit word |
| `SW` | Store 32-bit word |
| `BEQ` | Branch if equal |
| `BNE` | Branch if not equal |
| `J` | Jump |
| `JALR` | Jump and link register |
| `SLLV` | Shift left logical variable |

This list is not necessarily exhaustive of the instructions currently present in the source code. The CPU remains incomplete, and instruction behavior must be validated against the MIPS R3000A architecture.

### BIOS Loading and Execution

The emulator supports loading a BIOS image from a user-provided path.

The previously tested `scph1001.bin` BIOS image was 512 KiB in size. During the initial loading test, the first instruction read was:

    0x3C080013

This decodes to:

    LUI $t0, 0x0013

That initial test only confirmed that the BIOS image could be loaded and its first instruction read.

Subsequent development has progressed to executing BIOS code and reaching an apparent idle loop after approximately 50 million execution steps.

Although this is a significant milestone, it does not establish full BIOS compatibility or complete emulation of the original hardware.

### DMA and Memory-Mapped Hardware

The emulator has been extended to handle selected DMA-related register accesses that previously stopped BIOS execution.

Relevant registers include:

- DMA Interrupt Register (`DICR`).
- DMA channel control registers (`CHCR`).
- DMA block control registers (`BCR`).
- DMA memory address registers (`MADR`).

Register access support does not necessarily imply that actual DMA transfers, interrupt signaling, or timing behavior are correctly implemented.

The DMA subsystem needs further work to reproduce hardware behavior accurately.

## Requirements

- Working Linux environment
- GCC
- Standard C development tools
- A legally obtained PlayStation 1 BIOS image for BIOS testing

The BIOS image used in previous tests is expected to be exactly 512 KiB (524,288 bytes).

## Build

Clone the repository and enter the project directory:

    git clone https://github.com/imafancylad/psx-emu
    cd psx-emu

Compile the current source files:

    gcc -Wall -Wextra -std=c11 main.c cpu.c memory.c -o psx-emu

For development builds with AddressSanitizer and UndefinedBehaviorSanitizer:

    gcc -std=c11 -Wall -Wextra -O2 -g \
        -fsanitize=address,undefined \
        main.c cpu.c memory.c -o psx-emu

The build commands may need to be updated as new source files and hardware components are added.

## Run

Run the current built-in CPU and memory tests:

    ./psx-emu

Load a BIOS image using the existing BIOS-loading interface:

    ./psx-emu "$HOME/bios.bin"

Replace the path with the location of your own BIOS image.

The exact runtime behavior depends on the current implementation in `main.c`. BIOS execution, test mode, and executable loading should be documented separately as those interfaces are developed.

### Running a PS-EXE

Direct PS-EXE loading is the next planned milestone.

The intended interface is:

    ./psx-emu game.iso

Once implemented, the loader should validate the executable header, copy the payload into emulated RAM, initialize the CPU state, and begin execution at the entry point.

Until this feature has been implemented and tested, the command above is a planned interface rather than a guaranteed working command.

## Testing and Diagnostics

BIOS execution can be monitored through the emulator's diagnostic output.

Example commands for reviewing logs, assuming the emulator writes diagnostics to standard error and BIOS output to standard output:

    ./psx-emu bios.bin > bios_out.log 2> bios_err.log

Search for failures and unsupported instructions:

    grep -Ei "fail|UNKNOWN|BUCLE" bios_err.log | head -20

Inspect the latest execution steps:

    grep "^step=" bios_err.log | tail -20

Review the latest BIOS output:

    tail -n 60 bios_out.log

These commands depend on the diagnostic format used by the current source code.

When investigating a failure, record:

- The execution step.
- The current program counter (`PC`).
- The instruction word.
- The failing memory address, if applicable.
- The relevant register values.
- Whether the failure is reproducible.

## Project Structure

The current core project structure includes:

    psx-emu/
    ├── main.c       # Program entry point and execution/test harness
    ├── cpu.h        # CPU structure and public function declarations
    ├── cpu.c        # CPU instruction execution
    ├── memory.h     # Memory constants, structures, and declarations
    └── memory.c     # RAM, BIOS mapping, and memory access implementation

The structure may change as the emulator grows to include additional components such as the GPU, DMA controller, timers, interrupts, CD-ROM controller, and SPU.

## Known Limitations

The emulator is still under active development. Major missing or incomplete features include:

- Full MIPS R3000A instruction coverage and accurate CPU behavior.
- Complete branch delay-slot behavior and CPU edge cases.
- Exceptions and Coprocessor 0 behavior.
- Accurate interrupt controller behavior.
- Complete BIOS service emulation.
- Accurate hardware timing and coordination between components.
- GPU command processing and framebuffer rendering.
- Complete DMA transfer behavior.
- Timers and event scheduling.
- CD-ROM controller and disc image support.
- SPU and audio output.
- Controller input.
- Memory-mapped hardware register accuracy.
- PS-EXE loading and execution, until the loader is implemented and validated.
- Commercial game booting and compatibility testing.

Reaching the BIOS idle loop does not guarantee that games will execute correctly.

Unsupported instructions and hardware accesses should produce useful diagnostic information rather than being silently treated as successful operations.

## Development Roadmap

Development will continue incrementally, with focused changes and tests at each stage.

1. Implement a PS-EXE loader.
2. Validate the executable header and safely load program data into RAM.
3. Initialize the CPU entry point and relevant registers.
4. Run a minimal test executable to validate CPU execution and memory access.
5. Fix missing CPU instructions and memory mapping issues exposed by test programs.
6. Implement GPU command processing and framebuffer output.
7. Improve DMA transfers and interrupt behavior.
8. Implement timers and event scheduling.
9. Add controller input and additional hardware features.
10. Investigate CD-ROM access and booting games through the BIOS.
11. Expand testing with homebrew executables and, where possible, legally obtained test software.

Each milestone should include reproducible tests and clear diagnostics before being considered complete.

## Development Principles

- Implement features incrementally.
- Test each new behavior before relying on it.
- Keep existing tests passing as the implementation grows.
- Prefer clear diagnostics over silently ignoring unsupported behavior.
- Distinguish register access from complete hardware emulation.
- Distinguish BIOS execution from successful game execution.
- Do not treat an apparent idle loop as proof of complete hardware compatibility.
- Treat compatibility as something to demonstrate through testing, not as a promise.

## Contributing

This is a personal work-in-progress project. Suggestions, bug reports, and code contributions are welcome as the emulator develops.

Please keep changes focused and include tests or reproducible steps when possible. When reporting a bug, include the execution step, PC, instruction word, relevant memory address, and diagnostic output.

## BIOS and Copyright Notice

This repository does not include a PlayStation BIOS. BIOS firmware is proprietary material; obtain and use it only in accordance with applicable laws and the rights of its owner.

Do not commit BIOS files, commercial game images, or other copyrighted game data to the repository.

