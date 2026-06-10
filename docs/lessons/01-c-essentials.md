# Lesson 1 — Just Enough C (read alongside `app/`)

You know Python from data science. C is different in one fundamental way:
**nothing is managed for you.** No garbage collector, no dynamic lists, no
exceptions. You say exactly where every byte lives — which is precisely why
C runs on a chip with 64 KB of RAM.

## Types have fixed sizes

```c
#include <stdint.h>

int16_t  sample;   /* exactly 16 bits, signed:   -32768 .. 32767  */
uint32_t addr;     /* exactly 32 bits, unsigned:  0 .. 4294967295 */
```

Firmware code uses these `stdint.h` types instead of plain `int`, because
"how many bits is this?" must never depend on the compiler. Look at
`app/ringbuf.h` — every field has an explicit width.

## Headers (.h) vs sources (.c)

- `ringbuf.h` is the **interface**: what exists (types, function signatures).
- `ringbuf.c` is the **implementation**: how it works.

Other files `#include "ringbuf.h"` to use the ring buffer without knowing
its internals. The `#ifndef SENSORHUB_RINGBUF_H` guard at the top just
prevents the file being pasted twice into the same compilation.

## Pointers — the part everyone fears

A pointer is a variable that holds a memory address.

```c
int16_t x = 42;
int16_t *p = &x;   /* p holds the ADDRESS of x   (& = "address of")  */
*p = 99;           /* writes 99 INTO x           (* = "value at")    */
```

Why does firmware live on pointers?

1. **Functions can modify caller data.** `ringbuf_pop(rb, &v)` writes the
   popped item into your `v` through its address. (In Python everything is
   a reference anyway — C just makes it explicit.)
2. **Hardware IS addresses.** The UART on our chip is the bytes at address
   `0x4000C000`. You talk to it through a pointer. That's Lesson 3.

## Structs = data, functions = behavior

C has no classes. The pattern used in every module here is:

```c
typedef struct { ... } ringbuf_t;            /* the data            */
void ringbuf_init(ringbuf_t *rb, ...);       /* "methods" take the  */
bool ringbuf_push(ringbuf_t *rb, ...);       /*  struct's address   */
```

That is exactly `self`, spelled by hand. Read `app/stats.c` now — it's the
smallest module and uses everything from this lesson.

## Strings are bare arrays

A C string is an array of bytes ending in `'\0'`. There is no `len()`, no
bounds checking — walking off the end is the classic C bug. See how
`app/shell.c` carefully checks `len < SHELL_LINE_MAX - 1` before storing a
character: that missing check, in countless real products, is how buffer
overflow vulnerabilities are born.

## Exercise

In `app/fmt.c`, follow `fmt_i32(-42, buf)` line by line with pen and paper.
Why does the digit loop produce them backwards? Why is the negation done in
`uint32_t`? (Hint: what is -(-2147483648) in a 32-bit signed world?)
