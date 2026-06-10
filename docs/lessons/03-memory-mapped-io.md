# Lesson 3 — Memory-Mapped I/O and `volatile` (read alongside `target/uart.c`)

How does software make hardware *do* something? On almost every modern
chip: by reading and writing special memory addresses.

## Registers are just addresses

The UART peripheral on this chip occupies addresses starting at
`0x4000C000`. Address `0x4000C000` is its **data register**: write a byte
there and the UART serializes it out the wire. Address `0x4000C018` is its
**flag register**: bit 5 tells you "transmit buffer full".

`target/uart.c` wraps this in one macro:

```c
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define UART0_DR    REG32(0x4000C000u)
```

Unpack it from the inside out: take the number `0x4000C000`, cast it to
"pointer to volatile uint32" (`(volatile uint32_t *)`), then dereference it
(`*`). Now `UART0_DR = 'A';` is an ordinary C assignment — that transmits
the letter A. This is what "bare metal" literally means: no driver below
you, just the bus.

## Why `volatile` is non-negotiable

```c
void uart0_putc(char c)
{
    while (UART0_FR & FR_TXFF) { }   /* wait for space */
    UART0_DR = c;
}
```

To an optimizing compiler, that loop reads the same "variable" repeatedly
with no visible change — without `volatile` it would *cache the first read*
and either skip the loop or spin forever. `volatile` tells the compiler:
"every read and write here has side effects you cannot see; perform each
one, exactly, in order."

The rule: **anything that can change outside the compiler's view —
hardware registers, variables shared with an interrupt — must be
`volatile`.** This is the single most common embedded interview question,
and this project has both kinds (see `sampler_t.offset` for the
ISR-shared kind).

## Read-modify-write

Notice `SYSCTL_RCGC1 |= RCGC1_UART0;` — read the register, OR in one bit,
write it back. We set *only* the UART0 clock bit without disturbing the
others. Bitwise ops (`|`, `&`, `~`, `<<`) are how C touches individual
bits; there is no bit type.

## Where do the addresses come from?

The chip's datasheet/reference manual. Every peripheral chapter has a
register table: offset, name, meaning of each bit. The `#define`s at the
top of `uart.c` and `systick.c` are transcribed straight from the LM3S6965
datasheet — that transcription work is a real part of the embedded job
(usually wrapped in vendor headers, but you should know what's beneath).

## Exercise

In QEMU, the busy-wait in `uart0_putc` rarely loops because the emulated
FIFO drains instantly. Find the register and bit you would check to know
"a received byte is waiting" (look at `FR_RXFE`), and trace how
`UART0_Handler` uses it.
