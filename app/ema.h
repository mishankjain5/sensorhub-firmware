#ifndef SENSORHUB_EMA_H
#define SENSORHUB_EMA_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Exponential moving average filter, integer-only (no FPU on a Cortex-M3).
 *
 *     y(n) = y(n-1) + (x(n) - y(n-1)) / 8        (alpha = 1/8)
 *
 * The state is kept in Q4 fixed point (value * 16) so repeated integer
 * division does not grind the average to a halt far from the true mean.
 */

typedef struct {
    int32_t y_q4;    /* filtered value in Q4 (16ths of a unit) */
    bool primed;     /* first sample seeds the filter directly */
} ema_t;

void ema_init(ema_t *f);

/* Feed one raw sample; returns the current filtered value (integer units). */
int16_t ema_update(ema_t *f, int16_t x);

#endif
