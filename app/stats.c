#include "stats.h"

void stats_reset(stats_t *s)
{
    s->count = 0;
    s->min = 0;
    s->max = 0;
    s->sum = 0;
}

void stats_update(stats_t *s, int16_t v)
{
    if (s->count == 0) {
        s->min = v;
        s->max = v;
    } else {
        if (v < s->min) {
            s->min = v;
        }
        if (v > s->max) {
            s->max = v;
        }
    }
    s->sum += v;
    s->count++;
}

int32_t stats_mean(const stats_t *s)
{
    if (s->count == 0) {
        return 0;
    }
    return (int32_t)(s->sum / (int64_t)s->count);
}
