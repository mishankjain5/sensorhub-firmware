#include "fmt.h"

char *fmt_i32(int32_t v, char *buf)
{
    char tmp[12];
    int n = 0;
    char *p = buf;
    /* Work in unsigned so INT32_MIN does not overflow on negation. */
    uint32_t u = (uint32_t)v;

    if (v < 0) {
        *p++ = '-';
        u = (uint32_t)0 - u;
    }
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u != 0u);

    while (n > 0) {
        *p++ = tmp[--n];
    }
    *p = '\0';
    return buf;
}

bool fmt_parse_i32(const char *s, int32_t *out)
{
    bool neg = false;
    uint32_t acc = 0;
    /* Magnitude limit: 2147483647 for positive, 2147483648 for negative. */
    uint32_t limit;

    if (s == 0 || *s == '\0') {
        return false;
    }
    if (*s == '-') {
        neg = true;
        s++;
        if (*s == '\0') {
            return false;
        }
    }
    limit = neg ? 0x80000000u : 0x7FFFFFFFu;

    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        uint32_t digit = (uint32_t)(*s - '0');
        if (acc > (limit - digit) / 10u) {
            return false;        /* would overflow */
        }
        acc = acc * 10u + digit;
        s++;
    }
    *out = neg ? (int32_t)(0u - acc) : (int32_t)acc;
    return true;
}
