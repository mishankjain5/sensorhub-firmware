#ifndef SENSORHUB_STATS_H
#define SENSORHUB_STATS_H

#include <stdint.h>

/* Running min / max / mean over all samples since the last reset.
 * The sum is 64-bit: at 100 samples/s a 32-bit sum of 10-bit values
 * would overflow in under a year — 64-bit makes the question moot. */

typedef struct {
    uint32_t count;
    int16_t min;
    int16_t max;
    int64_t sum;
} stats_t;

void stats_reset(stats_t *s);
void stats_update(stats_t *s, int16_t v);
int32_t stats_mean(const stats_t *s);   /* 0 when no samples yet */

#endif
