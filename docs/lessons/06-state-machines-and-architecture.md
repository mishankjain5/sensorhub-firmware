# Lesson 6 — State Machines & Firmware Architecture (read alongside `app/alarm.c` and `target/main.c`)

## Finite-state machines: the firmware workhorse

Most firmware behavior is "what I do depends on what mode I'm in" —
exactly what an FSM encodes. `app/alarm.c` is the pattern in miniature:

- **States** as an `enum` (`NORMAL`, `WARN`, `CRIT`) — never booleans like
  `is_warning && !is_critical` that can combine into nonsense.
- **One update function** holding a `switch` on the current state. Every
  transition is explicit and findable; nothing changes state anywhere else.
- **Hysteresis**: to *leave* a state, the value must drop a margin *below*
  the threshold that entered it. Without this, a value hovering at 620
  would flip NORMAL↔WARN at 100 Hz — spamming logs, relays, or pagers.
  Run `tests/test_main.c::test_alarm_no_chatter` to see it defended.

Same pattern scales to real products: protocol parsers, motor controllers,
charging logic, Bluetooth pairing — all switch-on-enum FSMs, just bigger.

## The superloop architecture

`main()` is a "superloop" — the simplest firmware architecture and still
the most common for small devices:

```
forever:
    drain UART ring buffer  -> feed shell
    drain sample ring buffer -> filter -> stats -> alarm FSM
    sleep until next interrupt (wfi)
```

Interrupts produce events into queues; the loop consumes them. No RTOS, no
threads — and therefore no deadlocks, no priority inversion, almost
nothing nondeterministic. When products outgrow this (many independent
deadlines), they move to an RTOS like FreeRTOS or Zephyr, where each
pipeline stage becomes a task. Knowing *when* a superloop suffices is
itself an interview-grade judgment call.

## The portable-core architecture

The repository's deepest design decision:

```
app/     pure C99, zero hardware knowledge  <- unit-tested on the PC
target/  startup, linker script, registers  <- the only hardware code
```

`app/` modules touch hardware **only through function pointers handed in
by main()** (see how `shell_init` receives `uart0_write`). Consequences:

- 2,000+ assertions run on every host build, no board needed (`ctest`).
- Porting to a different MCU = rewriting `target/` (~300 lines), zero
  changes to the logic.
- Code review is easier: anything touching a register is in one folder.

This "hardware abstraction boundary" is how serious firmware teams work,
and being able to articulate *why* is worth more in an interview than any
single API you could memorize.

## Where to go next with this project

Each of these is a meaningful, interview-discussable extension:

1. **Watchdog**: kick a (simulated) watchdog in the superloop; explain what
   it rescues you from.
2. **Binary protocol**: replace the text shell with length-prefixed,
   CRC-checked frames — closer to production telemetry.
3. **Second sensor channel**: generalize sampler/stats/alarm into arrays —
   feel how static allocation shapes API design.
4. **Real hardware**: any $5 STM32/RP2040 board. `app/` ports unchanged;
   you rewrite `target/` against the new datasheet — the exact skill
   employers want demonstrated.
