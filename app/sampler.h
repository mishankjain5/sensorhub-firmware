#ifndef SENSORHUB_SAMPLER_H
#define SENSORHUB_SAMPLER_H

#include <stdint.h>

/*
 * Synthetic sensor standing in for an ADC channel.
 *
 * Produces  baseline + sine wave + pseudo-random noise + injected offset,
 * all in integer math (a sine lookup table instead of sinf() — the same
 * trick used on MCUs without a floating-point unit).
 *
 * With the firmware's 100 Hz tick the 100-step wave period gives a 1 Hz
 * signal: baseline 500, amplitude 80, noise within ±8 units.
 *
 * `offset` is written by the shell ("sim offset <n>") and read by the
 * SysTick interrupt — hence volatile. A single aligned 16-bit store is
 * atomic on Cortex-M3, so no further locking is needed.
 */

typedef struct {
    uint16_t phase;            /* 0..99 position in the wave table */
    uint32_t rng;              /* LCG state for noise */
    volatile int16_t offset;   /* fault-injection offset from the shell */
} sampler_t;

void sampler_init(sampler_t *s, uint32_t seed);
int16_t sampler_next(sampler_t *s);             /* called from the ISR */
void sampler_set_offset(sampler_t *s, int16_t offset);

#endif
