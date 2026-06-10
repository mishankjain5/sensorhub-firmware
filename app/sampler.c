#include "sampler.h"

#define BASELINE   500
#define WAVE_STEPS 100

/* Quarter sine table: round(80 * sin(k * 90deg / 25)) for k = 0..25.
 * The other three quadrants are derived by mirroring, so 26 entries
 * describe the whole 100-step wave. */
static const int16_t quarter_sine[26] = {
     0,  5, 10, 15, 20, 25, 29, 34, 39, 43, 47, 51, 55,
    58, 62, 65, 68, 70, 72, 74, 76, 77, 79, 79, 80, 80
};

static int16_t wave(uint16_t phase)
{
    if (phase <= 25) {
        return quarter_sine[phase];
    }
    if (phase <= 50) {
        return quarter_sine[50 - phase];
    }
    if (phase <= 75) {
        return (int16_t)-quarter_sine[phase - 50];
    }
    return (int16_t)-quarter_sine[100 - phase];
}

void sampler_init(sampler_t *s, uint32_t seed)
{
    s->phase = 0;
    s->rng = seed;
    s->offset = 0;
}

static int16_t noise(sampler_t *s)
{
    /* Linear congruential generator; the high bits are the most random. */
    s->rng = s->rng * 1103515245u + 12345u;
    return (int16_t)((s->rng >> 16) % 17u) - 8;   /* -8 .. +8 */
}

int16_t sampler_next(sampler_t *s)
{
    int16_t v = (int16_t)(BASELINE + wave(s->phase) + noise(s) + s->offset);

    s->phase++;
    if (s->phase == WAVE_STEPS) {
        s->phase = 0;
    }
    return v;
}

void sampler_set_offset(sampler_t *s, int16_t offset)
{
    s->offset = offset;
}
