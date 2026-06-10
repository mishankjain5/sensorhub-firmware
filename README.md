# SensorHub — bare-metal ARM Cortex-M3 firmware, no hardware required

Firmware for a sensor-monitoring node, written in C99 from the reset vector
up: hand-written startup code, vector table, linker script, and
register-level drivers for the TI Stellaris LM3S6965 (ARM Cortex-M3). It
runs on QEMU's emulation of that chip, so the whole thing builds, boots,
and demos on a plain PC.

```
            ┌────────────────────── firmware ──────────────────────┐
 SysTick    │  sensor ──> ring buffer ──> EMA filter ──> stats     │
 IRQ 100Hz  │  (synthetic ADC)  │              │            │      │
            │                   │              └──> alarm FSM      │
 UART RX    │                   │         NORMAL ⇄ WARN ⇄ CRIT     │
 IRQ        │  ring buffer ──> command shell <───────┘             │
            └────────────────────────│─────────────────────────────┘
                                UART0 (your terminal)
```

What it does, live in your terminal:

- samples a synthetic sensor at 100 Hz from the **SysTick interrupt**
- smooths it with an **integer-only EMA filter** (Q4 fixed point — no FPU)
- tracks min/max/mean and feeds a **hysteresis alarm state machine**
- serves an **interrupt-driven UART command shell**: query readings,
  retune thresholds, and inject faults while it runs

## Embedded credentials, not just C that compiles

| Concern | Where | What it demonstrates |
|---|---|---|
| Boot | `target/startup.c` | vector table, reset handler, .data copy / .bss zero |
| Memory layout | `target/lm3s6965.ld` | hand-written linker script, flash/SRAM split |
| Register I/O | `target/uart.c`, `target/systick.c` | `volatile` MMIO from the datasheet, NVIC setup |
| ISR ↔ main-loop safety | `app/ringbuf.c` | lock-free SPSC queue, overflow accounting |
| No heap | everywhere | all storage static; build prints exact RAM use (540 B) |
| No floats | `app/ema.c`, `app/sampler.c` | Q4 fixed point, quarter-wave sine lookup table |
| Robust control | `app/alarm.c` | switch-on-enum FSM with hysteresis (no chatter) |
| Testability | `app/` vs `target/` split | portable core, 2,000+ host-run assertions via CTest |

## Build

Requirements: CMake, a host GCC (tests), `arm-none-eabi-gcc` (firmware),
QEMU (`qemu-system-arm`). On Windows:
`winget install Arm.GnuArmEmbeddedToolchain SoftwareFreedomConservancy.QEMU Kitware.CMake BrechtSanders.WinLibs.POSIX.UCRT`

```powershell
# firmware (cross-compile)
cmake -S . -B build-arm -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build-arm          # prints flash/RAM usage when done

# unit tests (host)
cmake -S . -B build-host -G "MinGW Makefiles"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

(On Linux/macOS drop the `-G "MinGW Makefiles"`.)

## Run it

```powershell
qemu-system-arm -M lm3s6965evb -nographic -kernel build-arm/sensorhub.elf
```

Type `help` at the prompt. Exit QEMU with `Ctrl+A`, then `X`.

### Session transcript (real output)

```
SensorHub v1.0 - bare-metal ARM Cortex-M3 (LM3S6965)
100 Hz sampling | EMA filter | alarm FSM | type 'help'

> read
raw=545 filt=556 state=NORMAL
> alarm
state=NORMAL warn=620 crit=680 hyst=10
> sim offset 150          <- inject a fault into the sensor
offset=150

[ALARM] NORMAL -> WARN at filt=621
[ALARM] WARN -> CRIT at filt=682
> sim offset 0            <- remove the fault
offset=0

[ALARM] WARN -> NORMAL at filt=596
> stats
count=1159 min=427 max=723 mean=540 lost_samples=0 rx_overruns=0
```

## Layout

```
app/      portable firmware logic, pure C99, no hardware includes
          ringbuf, ema, alarm, stats, sampler, shell, fmt
target/   everything hardware: startup.c, lm3s6965.ld, uart.c,
          systick.c, main.c (the superloop)
tests/    host-side unit tests (CTest)
docs/     lessons/ 01-06 — a study track through the codebase
          INTERVIEW.md — Q&A grounded in this code
cmake/    arm-none-eabi toolchain file
```

The design rule that shapes everything: **`app/` never touches hardware.**
It receives function pointers (e.g. the shell gets a `write` function) and
is compiled twice — for the Cortex-M3 image and for the host test binary.
Porting to real silicon (STM32, RP2040, ...) means rewriting `target/`
only.

## Docs

Start with `docs/lessons/01-c-essentials.md` and read in order — each
lesson explains one embedded concept against the actual source files, with
an exercise.
