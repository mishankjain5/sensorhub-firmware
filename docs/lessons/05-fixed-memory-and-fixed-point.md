# Lesson 5 — No Heap, No Floats (read alongside `app/ema.c` and `app/sampler.c`)

Two habits from desktop programming get unlearned in firmware: allocating
memory whenever you like, and doing math in floating point.

## Why no `malloc`

Every buffer in this project is a fixed-size global or stack array:

```c
static rb_item_t g_sample_storage[64];   /* decided at compile time */
```

Reasons firmware avoids the heap:

- **Fragmentation**: with 64 KB of RAM, alternating allocations of mixed
  sizes can leave you unable to allocate even when free bytes exist. A
  device that must run for 10 years cannot risk that.
- **Determinism**: `malloc` takes a variable, potentially unbounded time —
  poison for code with real-time deadlines.
- **Failure handling**: what does your sensor do when `malloc` returns
  NULL, three years into deployment, in a substation? With static
  allocation the question disappears: if it links, the memory exists.

Bonus: the build prints exact RAM usage (`bss` = 540 bytes) because
everything is statically known. Safety standards (automotive MISRA C,
aviation DO-178) outright ban or heavily restrict heap use.

## Why no `float`

The Cortex-M3 has **no floating-point unit**. Writing `0.125 * x` compiles,
but secretly links in software emulation: hundreds of cycles per multiply
and kilobytes of library code. So firmware does fixed-point arithmetic —
integers with an implicit scale factor.

### Trick 1: scale-and-shift (the EMA filter)

We want `y += (x - y) / 8` but repeated integer division loses the
fractional remainder every step. Instead `app/ema.c` keeps the state
multiplied by 16 ("Q4 format" — 4 binary places after the point):

```c
f->y_q4 += ((x << 4) - f->y_q4) / 8;   /*16ths preserve the fraction */
return (f->y_q4 + 8) >> 4;             /* +8 rounds instead of truncating */
```

Same idea as doing money math in cents instead of euros.

### Trick 2: lookup tables (the sine wave)

`sin()` needs the math library and floats. `app/sampler.c` instead stores
26 precomputed values of a quarter sine wave and exploits symmetry to
cover all 360°. Costs 52 bytes of flash and ~3 cycles per sample. Wave
tables like this are everywhere in real DSP firmware.

### Trick 3: integer units

Values here are plain integer "sensor counts". Real meters do the same
with physical quantities: store millivolts as `int32_t` instead of volts
as `double`. You'll see `mV`, `mA`, `mW` fields all over production
telemetry code.

## Exercise

In `ema.c`, replace `(f->y_q4 + 8) >> 4` with `f->y_q4 >> 4` (drop the
rounding), rebuild the **host** tests (`cmake --build build-host` then
`ctest`). Which test fails and what value does it converge to? That
off-by-one is exactly the kind of bug fixed-point code breeds — and why
the test exists.
