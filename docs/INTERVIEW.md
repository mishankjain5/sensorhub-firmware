# Interview Preparation — talking about SensorHub

Answers are grounded in this repository; every claim can be shown in code
during a screen-share. Practice saying these out loud.

---

**Q1. Walk me through your project in one minute.**

> SensorHub is bare-metal firmware for an ARM Cortex-M3, developed and
> demonstrated entirely on QEMU. A SysTick interrupt samples a synthetic
> sensor at 100 Hz into a lock-free ring buffer; the main loop filters the
> signal with an integer EMA, tracks statistics, and runs a hysteresis
> alarm state machine; a second interrupt feeds a UART command shell so you
> can query the device and inject faults live. The logic is split into a
> portable C99 core with no hardware dependencies — unit-tested on the PC
> with 2,000+ assertions — and a thin target layer with the startup code,
> vector table, linker script, and register-level drivers I wrote from the
> datasheet.

**Q2. What happens on this chip between power-on and your `main()`?**

> The Cortex-M3 reads two words from flash address 0: the initial stack
> pointer (I set it to the top of SRAM in the linker script) and the reset
> vector. My `Reset_Handler` then copies the `.data` section's initial
> values from flash to SRAM, zero-fills `.bss`, and calls `main()`. I wrote
> that by hand in `target/startup.c` — there's no CRT or OS doing it for me.

**Q3. What does `volatile` do, and where did you need it?**

> It forbids the compiler from caching or eliminating accesses to a
> variable, because the value can change outside the compiler's view. I
> need it in two places: memory-mapped registers — my UART busy-wait loop
> `while (UART0_FR & FR_TXFF)` would otherwise be optimized into reading
> the flag once — and data shared with ISRs, like the ring buffer indices
> and the fault-injection offset. Important nuance: volatile gives
> *visibility*, not *atomicity* — atomicity comes from using aligned
> 16-bit indices that Cortex-M3 reads and writes in one instruction.

**Q4. How does data get from your interrupt to the main loop safely?**

> Through a single-producer single-consumer ring buffer. The ISR only ever
> writes the head index, the main loop only writes the tail, so they never
> race on the same variable. The producer fills the slot before publishing
> the new head, so the consumer can't observe unwritten data. And both
> indices are volatile so the main loop actually re-reads them. No locks,
> no disabling interrupts, bounded ISR time.

**Q5. What happens when the buffer is full? When samples are lost?**

> Push fails and increments a `dropped` counter rather than overwriting or
> blocking — in a telemetry system you want loss to be *visible*, so the
> `stats` command reports `lost_samples` and `rx_overruns`. I actually
> watched this fire: pasting a burst of commands at boot overflowed my
> original 32-byte RX buffer, the counter caught it, and I sized the
> buffer up. That counter is how you notice the problem in the field.

**Q6. Why is there no `malloc` anywhere?**

> Long-running devices can't risk heap fragmentation, `malloc`'s timing is
> unbounded, and an allocation failure three years into deployment has no
> good recovery. Everything here is statically allocated, so the build
> output tells me exactly: 540 bytes of RAM. If it links, it runs.

**Q7. Why no floating point?**

> The M3 has no FPU — floats would be emulated in software at hundreds of
> cycles each. The filter uses Q4 fixed point: state is kept in 16ths so
> integer division doesn't lose the fraction, with rounding on output. The
> sine source is a 26-entry quarter-wave lookup table mirrored across
> quadrants. I have a unit test that fails if you remove the rounding —
> the filter converges one count low.

**Q8. Why hysteresis in the alarm?**

> A value hovering exactly at the threshold would otherwise toggle the
> state every sample — log spam at best; chattering relay at worst. Leaving
> a state requires dropping a margin below the entry threshold. There's a
> dedicated test that oscillates the value inside the band and asserts the
> state never flaps.

**Q9. How do you test firmware without hardware?**

> Architecture first: the application core has zero hardware includes —
> the shell, for instance, gets an output function pointer instead of
> calling the UART. So the same .c files compile on the PC, where CTest
> runs 2,000+ assertions on every module plus an end-to-end pipeline test
> (inject offset → assert the FSM reaches CRIT → remove → assert recovery).
> Integration testing happens in QEMU against the real interrupt and
> register behavior. On a real product the remaining layer — actual
> silicon quirks — would be hardware-in-the-loop tests.

**Q10. What's in your linker script and why did you write one?**

> It defines the two memory regions — 256 K flash at 0x0, 64 K SRAM at
> 0x20000000 — and assigns sections: vector table first in flash (the CPU
> demands it at address 0), then code and rodata; `.data` with its
> load-address-in-flash / run-address-in-RAM split; `.bss` in RAM; and it
> exports the boundary symbols my startup code uses for the copy and
> zero-fill loops.

**Q11. Why keep ISRs short? What's in yours?**

> While an ISR runs, other interrupts wait — long handlers cause missed
> UART bytes and sampling jitter. Mine are 1–3 lines: take the sample or
> byte, push to the queue, acknowledge, return. All filtering, state
> machine work, and printing happen in the main loop. Printing from an ISR
> is the classic mistake — my UART write busy-waits, which would stall
> every other interrupt.

**Q12. What's the difference between your superloop and an RTOS? When
would you switch?**

> Superloop: interrupts enqueue events, one loop drains them, `wfi` sleeps
> between — fully deterministic, no scheduler, right-sized for one device
> with one main data path. I'd reach for an RTOS when there are multiple
> independent timing domains — say network stack plus motor control plus
> UI — where blocking one would starve another, and I'd name FreeRTOS or
> Zephyr.

**Q13. Your sampling is fake. What changes with a real ADC?**

> Surprisingly little — that was the point of the transport seams. The
> SysTick handler would trigger/read the ADC peripheral instead of calling
> `sampler_next()`; everything downstream — ring buffer, filter, stats,
> alarm, shell — is unchanged. I'd add: configuring the ADC's own
> registers, handling its end-of-conversion interrupt, and calibrating
> raw counts to physical units.

**Q14. How would you port this to an STM32?**

> Rewrite `target/`: new linker script memory map, new vector table
> layout, and new register definitions for the STM32's USART and clocks —
> a few hundred lines against the reference manual. The `app/` core and
> its tests don't change at all. That separation is why the port is
> measured in days, not weeks.

**Q15. What was the hardest bug?**

> Honest answer: input bursts at boot. Scripted input arrived faster than
> the main loop drained it — the RX ring buffer overflowed and a command
> got truncated mid-line. The drop counters made it diagnosable in
> minutes: `rx_overruns` was nonzero, exactly matching the missing
> characters. Fix: size the RX buffer for worst-case burst, and surface
> both overflow counters in `stats`. Lesson I'd quote: queues fail at the
> *boundaries* (boot, burst, shutdown), and counters you add on day one
> pay for themselves.

**Q16. What don't you know yet, coming from data science?**

> Real hardware bring-up: oscilloscopes, JTAG debugging silicon that
> doesn't match the manual, EMC issues — QEMU can't teach those. What I've
> proven is the software half: I can read a datasheet, write registers,
> reason about interrupts and memory, and structure firmware for
> testability. And my data background is genuinely useful on the other
> side of telemetry — the filtering and anomaly logic here is where the
> two fields meet.

---

## Live demo script (90 seconds, two commands)

```powershell
cmake --build build-arm
qemu-system-arm -M lm3s6965evb -nographic -kernel build-arm/sensorhub.elf
```

Then narrate: `help` → `read` (live value) → `alarm` (thresholds) →
`sim offset 150` → watch NORMAL→WARN→CRIT fire → `sim offset 0` → recovery
→ `stats` (note `lost_samples=0`). Exit QEMU with `Ctrl+A` then `X`.
