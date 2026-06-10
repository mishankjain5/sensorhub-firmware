#include "ema.h"

void ema_init(ema_t *f)
{
    f->y_q4 = 0;
    f->primed = false;
}

int16_t ema_update(ema_t *f, int16_t x)
{
    int32_t x_q4 = (int32_t)x << 4;

    if (!f->primed) {
        f->y_q4 = x_q4;
        f->primed = true;
    } else {
        f->y_q4 += (x_q4 - f->y_q4) / 8;
    }
    /* Round to nearest when converting back to integer units. */
    return (int16_t)((f->y_q4 + 8) >> 4);
}
