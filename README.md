# PSX Emulator

A PlayStation 1 emulator written from scratch in **C**.

> **Status: Work in progress.** This project is in an early development
> stage. It currently contains a basic memory subsystem and a small
> subset of MIPS R3000A-style CPU instructions. It is **not yet capable
> of booting the PlayStation BIOS or running commercial games**.

The goal is to build the emulator incrementally, implementing and
testing hardware components as the project progresses. Compatibility
will depend on the features that are implemented and validated; full
compatibility is not guaranteed.

## Current Features

### Memory

-   2 MiB of RAM.
-   512 KiB BIOS image support.
-   Optional BIOS loading from a file.
-   BIOS image size validation.
-   Byte reads and RAM byte writes.
-   32-bit word reads and writes.
-   Alignment checks for 32-bit memory accesses.
-   Little-endian memory access.
-   Basic address translation for KSEG0 and KSEG1.
-   Basic RAM mirroring within the currently supported physical address
    region.
-   BIOS mapping at physical addresses `0x1FC00000`--`0x1FC7FFFF`.
-   Basic protection against writes to the BIOS and accesses to unmapped
    regions.

### CPU

The current CPU implementation includes:

-   32 general-purpose registers.
-   Program counter (`PC`).
-   Register zero (`$zero`) maintained as zero.
-   Single-step instruction execution through `cpu_step`.
-   Simplified branch delay-slot handling.
-   Basic jumps and conditional branches.

Implemented instructions:

  Instruction   Description
  ------------- -------------------------------------------------------------
  `ADDU`        Add unsigned, without an arithmetic overflow exception
  `SUBU`        Subtract unsigned, without an arithmetic overflow exception
  `AND`         Bitwise AND
  `OR`          Bitwise OR
  `ADDIU`       Add immediate, without an arithmetic overflow exception
  `LUI`         Load upper immediate
  `LW`          Load 32-bit word
  `SW`          Store 32-bit word
  `BEQ`         Branch if equal
  `BNE`         Branch if not equal
  `J`           Jump

This is a deliberately small instruction subset. The implementation does
not yet reproduce all CPU behavior or hardware details of the original
console.

### BIOS loading

The program can load a BIOS image from a user-provided path and read its
first instruction. For the tested `scph1001.bin` image, the first
instruction read was:

``` text
0x3C080013
```

This decodes to:

``` asm
LUI $t0, 0x0013
```

The `LUI` instruction is implemented. **Reading this instruction does
not mean that the BIOS is being executed.** The current program still
runs a synthetic CPU test program.

## Requirements

-   Ubuntu or another Linux environment.
-   GCC.
-   Standard C development tools.
-   A legally obtained PlayStation 1 BIOS image, if you want to test
    BIOS loading. The BIOS is not included in this repository.

The tested BIOS image is expected to be exactly **512 KiB (524,288
bytes)**.

## Build

Clone the repository and enter the project directory:

``` bash
git clone <repository-url>
cd psx-emu
```

Compile the current source files:

``` bash
gcc -Wall -Wextra -std=c11 main.c cpu.c memory.c -o psx-emu
```

## Run

Run the built-in CPU and memory tests:

``` bash
./psx-emu
```

Optionally, provide a path to a BIOS image:

``` bash
./psx-emu "$HOME/Downloads/scph1001.bin"
```

Replace the path with the location of your own BIOS file. The program
attempts to load the BIOS and display its first instruction, then runs
the synthetic CPU test.

## Confirmed Test Output

The following output has been observed from the current memory and CPU
test program:

``` text
KSEG0 read: 0xDEADBEEF
KSEG1 read: 0xDEADBEEF
RAM mirror read: 0xDEADBEEF
PSX emulator
Register t0: 5
Register t1: 5
Register t2: 99
Register t3: 7
Register t4: 42
Register t5: 8
Register t6: 11
Register t7: 33
Register s0: 0
Register s1: 55
Final PC: 0x00000038
Instructions executed: 12
CPU test passed.
```

When run with a BIOS path, the following additional messages have been
observed:

``` text
BIOS loaded successfully.
BIOS entry instruction: 0x3C080013
```

These results confirm the current test cases only. They do not
demonstrate BIOS booting, game compatibility, or complete hardware
emulation.

## Project Structure

``` text
psx-emu/
├── main.c       # Program entry point and current test harness
├── cpu.h        # CPU structure and public function declarations
├── cpu.c        # Basic CPU instruction execution
├── memory.h     # Memory constants, structure, and function declarations
└── memory.c     # RAM/BIOS mapping and memory access implementation
```

This structure reflects the project at its current stage and may change
as more components are added.

## Known Limitations

The emulator is still in an early stage. Major missing features include:

-   Most MIPS instructions and accurate CPU behavior.
-   Complete branch delay-slot behavior and CPU edge cases.
-   Exceptions and interrupts.
-   Coprocessor 0 (system control registers and related behavior).
-   Actual BIOS execution and boot sequence.
-   More complete hardware timing and inter-component coordination.
-   GPU and graphics command processing.
-   DMA.
-   Timers.
-   CD-ROM controller.
-   SPU and audio.
-   Controller input.
-   Additional hardware registers, peripherals, and compatibility
    details.

Unknown or unsupported instructions must not be silently treated as
successfully executed. A future BIOS execution mode should report
unsupported instructions with useful diagnostic information, such as the
current `PC` and instruction word.

## Development Roadmap

Development will proceed incrementally, with small changes and tests at
each stage:

1.  Inspect the first 64 bytes of the BIOS image and decode the initial
    instructions.
2.  Identify a useful next MIPS instruction to implement.
3.  Add the instruction and create a focused test for it.
4.  Rebuild and rerun existing memory and CPU tests to check for
    regressions.
5.  Expand CPU coverage and improve architectural accuracy.
6.  Add appropriate exception and system-control behavior.
7.  Introduce a separate, controlled BIOS execution mode only when
    enough required functionality exists.
8.  Implement additional hardware components and validate them with
    targeted tests.

The immediate next step is to inspect the beginning of the BIOS image:

``` bash
od -An -tx4 -N64 "$HOME/Descargas/SCPH1001.BIN"
```

Alternatively:

``` bash
xxd -g1 -l 64 "$HOME/Descargas/SCPH1001.BIN"
```

The output should be reviewed before deciding which instruction to
implement next. The BIOS should not be run indefinitely with an
incomplete CPU implementation.

## Development Principles

-   Implement features incrementally.
-   Test each new behavior before relying on it.
-   Keep existing tests passing as the implementation grows.
-   Prefer clear diagnostics over silently ignoring unsupported
    behavior.
-   Do not confuse synthetic CPU tests with real BIOS execution.
-   Treat compatibility as something to demonstrate through testing, not
    as a promise.

## Contributing

This is a personal work-in-progress project. Suggestions, bug reports,
and code contributions may be useful as the emulator develops. Please
keep changes focused and include tests or reproducible steps when
possible.

## BIOS and Copyright Notice

This repository does not include a PlayStation BIOS. BIOS firmware is
proprietary material; obtain and use it only in accordance with
applicable laws and the rights of its owner. Do not commit BIOS files or
other copyrighted game data to the repository.

