# Lesson 2 — How a Microcontroller Boots (read alongside `target/startup.c` and `target/lm3s6965.ld`)

On your PC, the OS loads your program, sets up memory, and calls `main()`.
On a microcontroller **there is no OS** — this project contains every
instruction the CPU executes from power-on. Here's the whole story.

## The memory map

The LM3S6965 has two memories at fixed addresses:

```
0x00000000  ─ FLASH, 256 KB ─  your code + constants (survives power-off)
0x20000000  ─ SRAM,   64 KB ─  variables + stack     (lost at power-off)
0x40000000  ─ peripherals   ─  UART, timers... (Lesson 3)
0xE000E000  ─ Cortex-M core ─  NVIC, SysTick
```

The linker script `target/lm3s6965.ld` is where we tell the linker about
this. The `MEMORY` block defines the two regions, and `SECTIONS` says what
goes where.

## What the hardware does at reset

A Cortex-M CPU does exactly two things when power arrives:

1. Reads the 32-bit word at address `0x00000000` → loads it into the
   **stack pointer**.
2. Reads the word at `0x00000004` → jumps to that address.

That's why `startup.c` puts `vector_table[]` in the `.isr_vector` section
and the linker script places that section first in flash: entry 0 is the
top-of-RAM address, entry 1 is `Reset_Handler`. The table after that is the
interrupt vector table — one function address per interrupt (Lesson 4).

## Why Reset_Handler exists: the .data/.bss trick

Consider these globals:

```c
int16_t threshold = 620;   /* initialized   -> .data section */
int16_t last_value;        /* uninitialized -> .bss section  */
```

`threshold` must start as 620 — but RAM is random garbage at power-on, and
flash is read-only so the variable can't live there. The solution every
firmware uses:

- The **initial value** (620) is stored in flash.
- The **variable itself** lives in RAM.
- At reset, before `main()`, a loop copies all initial values from flash
  to RAM. A second loop fills `.bss` with zeros (C guarantees
  uninitialized globals are zero).

That's the entire body of `Reset_Handler()`. The funny symbols it uses
(`_sidata`, `_sdata`, `_edata`...) are addresses computed by the linker
script — find each one in `lm3s6965.ld`.

## The map file

After building, open `build-arm/sensorhub.map` and search for `g_samples`.
You'll see the actual SRAM address where the linker placed the ring buffer.
Nothing is hidden — you can account for every byte. The build also prints:

```
   text    data     bss
   6136       0     540
```

= 6136 bytes of flash for code, 540 bytes of RAM for variables. The whole
firmware uses under 1% of the chip's RAM.

## Exercise

Add `int32_t magic = 0x1234;` as a global in `main.c`, rebuild, and find
it in the map file. Which section did it land in? Now remove `= 0x1234`
and rebuild — which section now, and why did `data` and `bss` change?
