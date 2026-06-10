#include "alarm.h"

void alarm_init(alarm_t *a, int16_t warn, int16_t crit, int16_t hyst)
{
    a->state = ALARM_NORMAL;
    a->warn = warn;
    a->crit = crit;
    a->hyst = hyst;
}

alarm_state_t alarm_update(alarm_t *a, int16_t v)
{
    switch (a->state) {
    case ALARM_NORMAL:
        if (v >= a->crit) {
            a->state = ALARM_CRIT;
        } else if (v >= a->warn) {
            a->state = ALARM_WARN;
        }
        break;

    case ALARM_WARN:
        if (v >= a->crit) {
            a->state = ALARM_CRIT;
        } else if (v < (int16_t)(a->warn - a->hyst)) {
            a->state = ALARM_NORMAL;
        }
        break;

    case ALARM_CRIT:
        if (v < (int16_t)(a->crit - a->hyst)) {
            /* Dropped out of CRIT; decide how far down we landed. */
            if (v < (int16_t)(a->warn - a->hyst)) {
                a->state = ALARM_NORMAL;
            } else {
                a->state = ALARM_WARN;
            }
        }
        break;

    default:
        a->state = ALARM_NORMAL;
        break;
    }
    return a->state;
}

const char *alarm_state_name(alarm_state_t s)
{
    switch (s) {
    case ALARM_NORMAL: return "NORMAL";
    case ALARM_WARN:   return "WARN";
    case ALARM_CRIT:   return "CRIT";
    default:           return "?";
    }
}
