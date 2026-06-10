#ifndef SENSORHUB_ALARM_H
#define SENSORHUB_ALARM_H

#include <stdint.h>

/*
 * Threshold alarm as an explicit finite-state machine.
 *
 *   NORMAL --(v >= warn)--> WARN --(v >= crit)--> CRIT
 *   WARN   --(v < warn - hyst)--> NORMAL
 *   CRIT   --(v < crit - hyst)--> WARN or NORMAL (re-evaluated)
 *
 * The hysteresis band stops the state from chattering when the value
 * hovers right at a threshold — a real-world detail interviewers ask about.
 */

typedef enum {
    ALARM_NORMAL = 0,
    ALARM_WARN   = 1,
    ALARM_CRIT   = 2
} alarm_state_t;

typedef struct {
    alarm_state_t state;
    int16_t warn;    /* enter WARN at or above this */
    int16_t crit;    /* enter CRIT at or above this */
    int16_t hyst;    /* must drop this far below a threshold to leave */
} alarm_t;

void alarm_init(alarm_t *a, int16_t warn, int16_t crit, int16_t hyst);

/* Feed one (filtered) value; returns the state after the update. */
alarm_state_t alarm_update(alarm_t *a, int16_t v);

const char *alarm_state_name(alarm_state_t s);

#endif
