#ifndef SENSORHUB_FMT_H
#define SENSORHUB_FMT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Tiny number<->string helpers so the firmware does not need printf.
 * Newlib's printf would pull kilobytes of code (and a heap) into the image;
 * on a small MCU you write — or generate — exactly what you need.
 */

/* Writes the decimal representation of v into buf (at least 12 bytes,
 * enough for "-2147483648" + NUL). Returns buf for call-site convenience. */
char *fmt_i32(int32_t v, char *buf);

/* Parses a decimal integer (optional leading '-'). Returns false on empty
 * input, non-digit characters, or overflow past the int32 range. */
bool fmt_parse_i32(const char *s, int32_t *out);

#endif
