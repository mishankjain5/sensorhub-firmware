# Lesson 4 — Interrupts (read alongside `target/main.c` and `target/uart.c`)

Polling ("is it ready yet? is it ready yet?") wastes power and misses
events. Interrupts invert control: the **hardware calls your function**
when something happens.

## The mechanics

1. The SysTick timer counts down to zero (every 10 ms in our build).
2. The CPU finishes its current instruction, pushes registers onto the
   stack, looks up entry 15 of the **vector table** (Lesson 2), and jumps
   to whatever address is there: our `SysTick_Handler()`.
3. The handler runs, returns, the CPU pops the registers and resumes the
   interrupted code — which never knows it was paused.

The NVIC (Nested Vectored Interrupt Controller) arbitrates when several
fire at once. We enable UART0's line in it with one register write
(`NVIC_EN0 = 1 << 5` — interrupt number 5, vector slot 16+5=21; check
`startup.c`).

## The golden rule: ISRs do almost nothing

Both our handlers are 1–3 lines:

```c
void SysTick_Handler(void)
{
    ringbuf_push(&g_samples, sampler_next(&g_sampler));
}
```

Grab the data, queue it, get out. Filtering, state machines, printing —
all happen later in the main loop. Why?

- While an ISR runs, other interrupts (same/lower priority) wait. A slow
  ISR means missed UART bytes and jittery sampling.
- Printing from an ISR (`uart0_write` busy-waits!) could block for
  milliseconds. That's how real products drop data.

## The shared-data problem

`g_samples` is written by the ISR and read by `main()`. The ISR can fire
**between any two instructions** of main — sharing data naively leads to
race conditions, the nastiest bugs in firmware (they reproduce once a week,
at a customer site).

Our defenses, worth reciting in an interview:

1. **SPSC ring buffer** (`app/ringbuf.c`): the producer (ISR) only writes
   `head`; the consumer (main) only writes `tail`. Neither modifies the
   other's variable, so no lock is needed —
2. **provided the index loads/stores are atomic**, which aligned 16-bit
   accesses on Cortex-M3 are (a 64-bit counter would NOT be — main could
   read half-updated garbage).
3. **Ordering**: `ringbuf_push` writes the slot *first*, then publishes by
   updating `head`. The consumer can never see an index that points at
   unwritten data.
4. **`volatile` on head/tail/offset**: forces the compiler to actually
   perform each access instead of caching them in registers across the
   loop (Lesson 3).

The heavyweight alternative — disable interrupts around every access —
works but adds latency; the lock-free queue avoids it entirely.

## Sleep instead of spin

The main loop ends with `wfi` ("wait for interrupt"): the CPU core halts
until the next interrupt instead of burning power in a busy loop. On a
battery-powered sensor that's the difference between days and months.

## Exercise

Predict: if you put a 50 ms busy-wait inside `SysTick_Handler`, what
happens to (a) typed characters, (b) the sample rate? Then reason through
why `lost_samples` in the `stats` command would or wouldn't grow.
