/*
 * Host-side unit tests for the portable app/ modules.
 *
 * Because app/ never touches hardware, the exact same .c files that run on
 * the Cortex-M3 are compiled here for the PC and exercised with a plain
 * assert-style harness (no framework needed). This is the standard way to
 * test firmware logic without a board on your desk.
 */

#include <stdio.h>
#include <string.h>

#include "../app/alarm.h"
#include "../app/ema.h"
#include "../app/fmt.h"
#include "../app/ringbuf.h"
#include "../app/sampler.h"
#include "../app/shell.h"
#include "../app/stats.h"

static int tests_run;
static int tests_failed;

#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* ---------------- ringbuf ---------------- */

static void test_ringbuf(void)
{
    rb_item_t storage[4];   /* capacity 4 -> holds at most 3 items */
    ringbuf_t rb;
    rb_item_t v;

    ringbuf_init(&rb, storage, 4);
    CHECK(ringbuf_is_empty(&rb));
    CHECK(ringbuf_count(&rb) == 0);
    CHECK(!ringbuf_pop(&rb, &v));            /* pop from empty fails */

    CHECK(ringbuf_push(&rb, 10));
    CHECK(ringbuf_push(&rb, 20));
    CHECK(ringbuf_push(&rb, 30));
    CHECK(ringbuf_count(&rb) == 3);

    CHECK(!ringbuf_push(&rb, 40));           /* full: rejected... */
    CHECK(rb.dropped == 1);                  /* ...and counted */

    CHECK(ringbuf_pop(&rb, &v) && v == 10);  /* FIFO order */
    CHECK(ringbuf_pop(&rb, &v) && v == 20);

    CHECK(ringbuf_push(&rb, 50));            /* wraps around storage */
    CHECK(ringbuf_push(&rb, 60));
    CHECK(ringbuf_count(&rb) == 3);

    CHECK(ringbuf_pop(&rb, &v) && v == 30);
    CHECK(ringbuf_pop(&rb, &v) && v == 50);
    CHECK(ringbuf_pop(&rb, &v) && v == 60);
    CHECK(ringbuf_is_empty(&rb));
}

static void test_ringbuf_long_wrap(void)
{
    rb_item_t storage[8];
    ringbuf_t rb;
    rb_item_t v;
    int i;

    ringbuf_init(&rb, storage, 8);
    /* Push/pop far more items than the capacity: indices must wrap cleanly. */
    for (i = 0; i < 1000; i++) {
        CHECK(ringbuf_push(&rb, (rb_item_t)i));
        CHECK(ringbuf_pop(&rb, &v));
        if (v != (rb_item_t)i) {
            CHECK(0);
            return;
        }
    }
    CHECK(rb.dropped == 0);
}

/* ---------------- fmt ---------------- */

static void test_fmt_i32(void)
{
    char buf[12];

    CHECK(strcmp(fmt_i32(0, buf), "0") == 0);
    CHECK(strcmp(fmt_i32(7, buf), "7") == 0);
    CHECK(strcmp(fmt_i32(-1, buf), "-1") == 0);
    CHECK(strcmp(fmt_i32(12345, buf), "12345") == 0);
    CHECK(strcmp(fmt_i32(2147483647, buf), "2147483647") == 0);
    CHECK(strcmp(fmt_i32((int32_t)-2147483648, buf), "-2147483648") == 0);
}

static void test_fmt_parse(void)
{
    int32_t v;

    CHECK(fmt_parse_i32("0", &v) && v == 0);
    CHECK(fmt_parse_i32("620", &v) && v == 620);
    CHECK(fmt_parse_i32("-150", &v) && v == -150);
    CHECK(fmt_parse_i32("2147483647", &v) && v == 2147483647);
    CHECK(fmt_parse_i32("-2147483648", &v) && v == (int32_t)-2147483648);

    CHECK(!fmt_parse_i32("", &v));           /* empty */
    CHECK(!fmt_parse_i32("-", &v));          /* sign only */
    CHECK(!fmt_parse_i32("12x", &v));        /* trailing junk */
    CHECK(!fmt_parse_i32("1.5", &v));        /* not an integer */
    CHECK(!fmt_parse_i32("2147483648", &v)); /* overflow */
    CHECK(!fmt_parse_i32("-2147483649", &v));/* underflow */
}

/* ---------------- ema ---------------- */

static void test_ema(void)
{
    ema_t f;
    int16_t y = 0;
    int i;

    ema_init(&f);
    CHECK(ema_update(&f, 500) == 500);       /* first sample seeds directly */

    /* Constant input: must converge to that value and stay there. */
    for (i = 0; i < 100; i++) {
        y = ema_update(&f, 600);
    }
    CHECK(y == 600);

    /* Step response: one update moves 1/8 of the distance. */
    ema_init(&f);
    ema_update(&f, 0);
    y = ema_update(&f, 800);
    CHECK(y == 100);

    /* Negative values work too. */
    ema_init(&f);
    for (i = 0; i < 100; i++) {
        y = ema_update(&f, -250);
    }
    CHECK(y == -250);
}

/* ---------------- alarm ---------------- */

static void test_alarm_transitions(void)
{
    alarm_t a;

    alarm_init(&a, 620, 680, 10);
    CHECK(a.state == ALARM_NORMAL);

    CHECK(alarm_update(&a, 500) == ALARM_NORMAL);
    CHECK(alarm_update(&a, 619) == ALARM_NORMAL);  /* just below warn */
    CHECK(alarm_update(&a, 620) == ALARM_WARN);    /* at warn */
    CHECK(alarm_update(&a, 680) == ALARM_CRIT);    /* at crit */
    CHECK(alarm_update(&a, 700) == ALARM_CRIT);
    CHECK(alarm_update(&a, 675) == ALARM_CRIT);    /* inside hysteresis band */
    CHECK(alarm_update(&a, 669) == ALARM_WARN);    /* below crit - hyst */
    CHECK(alarm_update(&a, 615) == ALARM_WARN);    /* inside hysteresis band */
    CHECK(alarm_update(&a, 609) == ALARM_NORMAL);  /* below warn - hyst */
}

static void test_alarm_jumps(void)
{
    alarm_t a;

    /* NORMAL straight to CRIT on a big spike. */
    alarm_init(&a, 620, 680, 10);
    CHECK(alarm_update(&a, 900) == ALARM_CRIT);

    /* CRIT straight back to NORMAL on a big drop. */
    CHECK(alarm_update(&a, 100) == ALARM_NORMAL);
}

static void test_alarm_no_chatter(void)
{
    alarm_t a;
    int i;

    alarm_init(&a, 620, 680, 10);
    alarm_update(&a, 620);                   /* enter WARN */

    /* Value oscillating inside the hysteresis band must NOT flap states. */
    for (i = 0; i < 50; i++) {
        CHECK(alarm_update(&a, 618) == ALARM_WARN);
        CHECK(alarm_update(&a, 612) == ALARM_WARN);
    }
}

/* ---------------- stats ---------------- */

static void test_stats(void)
{
    stats_t s;

    stats_reset(&s);
    CHECK(stats_mean(&s) == 0);              /* no samples yet */

    stats_update(&s, 10);
    stats_update(&s, 20);
    stats_update(&s, -30);
    CHECK(s.count == 3);
    CHECK(s.min == -30);
    CHECK(s.max == 20);
    CHECK(stats_mean(&s) == 0);              /* (10+20-30)/3 */

    stats_reset(&s);
    stats_update(&s, 7);
    CHECK(s.count == 1 && s.min == 7 && s.max == 7 && stats_mean(&s) == 7);
}

/* ---------------- sampler ---------------- */

static void test_sampler(void)
{
    sampler_t s;
    int i;
    int16_t v;
    int16_t lo = 32767, hi = -32768;

    sampler_init(&s, 1234);
    for (i = 0; i < 1000; i++) {
        v = sampler_next(&s);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    /* baseline 500, wave +-80, noise +-8 -> everything in [412, 588] */
    CHECK(lo >= 412 && lo < 480);
    CHECK(hi <= 588 && hi > 520);

    /* Offset injection shifts the whole signal. */
    sampler_set_offset(&s, 1000);
    v = sampler_next(&s);
    CHECK(v >= 1412 && v <= 1588);
}

/* ---------------- shell ---------------- */

static char shell_output[256];
static int captured_argc;
static char captured_args[SHELL_MAX_ARGS][SHELL_LINE_MAX];

static void capture_out(const char *s)
{
    strncat(shell_output, s, sizeof(shell_output) - strlen(shell_output) - 1);
}

static void capture_cmd(int argc, char **argv, void *user)
{
    int i;

    (void)user;
    captured_argc = argc;
    for (i = 0; i < argc && i < SHELL_MAX_ARGS; i++) {
        strncpy(captured_args[i], argv[i], SHELL_LINE_MAX - 1);
    }
}

static void feed(shell_t *sh, const char *text)
{
    while (*text != '\0') {
        shell_input(sh, *text++);
    }
}

static void test_shell_basic(void)
{
    shell_t sh;

    shell_output[0] = '\0';
    captured_argc = -1;
    shell_init(&sh, capture_out, capture_cmd, 0);

    feed(&sh, "set warn 620\r");
    CHECK(captured_argc == 3);
    CHECK(strcmp(captured_args[0], "set") == 0);
    CHECK(strcmp(captured_args[1], "warn") == 0);
    CHECK(strcmp(captured_args[2], "620") == 0);
    CHECK(strstr(shell_output, "set warn 620") != 0);  /* echoed back */
}

static void test_shell_edge_cases(void)
{
    shell_t sh;
    int i;

    shell_init(&sh, capture_out, capture_cmd, 0);

    /* Empty line: handler must not run. */
    shell_output[0] = '\0';
    captured_argc = -1;
    feed(&sh, "\r");
    CHECK(captured_argc == -1);

    /* Whitespace-only line: handler must not run. */
    captured_argc = -1;
    feed(&sh, "   \r");
    CHECK(captured_argc == -1);

    /* Extra spaces between words collapse. */
    captured_argc = -1;
    feed(&sh, "  read   now  \r");
    CHECK(captured_argc == 2);
    CHECK(strcmp(captured_args[0], "read") == 0);
    CHECK(strcmp(captured_args[1], "now") == 0);

    /* Backspace edits the line. */
    captured_argc = -1;
    feed(&sh, "reax\b" "d\r");
    CHECK(captured_argc == 1);
    CHECK(strcmp(captured_args[0], "read") == 0);

    /* Backspace on an empty line is harmless. */
    captured_argc = -1;
    feed(&sh, "\b\b\bok\r");
    CHECK(captured_argc == 1);
    CHECK(strcmp(captured_args[0], "ok") == 0);

    /* Overlong line: extra chars dropped, no buffer overflow, still parses. */
    captured_argc = -1;
    shell_output[0] = '\0';
    for (i = 0; i < 200; i++) {
        shell_input(&sh, 'a');
    }
    shell_input(&sh, '\r');
    CHECK(captured_argc == 1);
    CHECK(strlen(captured_args[0]) == SHELL_LINE_MAX - 1);
}

/* ---------------- pipeline integration ---------------- */

static void test_pipeline_end_to_end(void)
{
    /* The same chain main.c runs, minus the hardware: a +150 offset on the
     * synthetic signal must eventually drive the alarm FSM into CRIT. */
    sampler_t smp;
    ema_t f;
    alarm_t a;
    alarm_state_t st = ALARM_NORMAL;
    int reached_crit = 0;
    int i;

    sampler_init(&smp, 42);
    ema_init(&f);
    alarm_init(&a, 620, 680, 10);
    sampler_set_offset(&smp, 150);

    for (i = 0; i < 500; i++) {
        st = alarm_update(&a, ema_update(&f, sampler_next(&smp)));
        if (st == ALARM_CRIT) {
            reached_crit = 1;
        }
    }
    CHECK(reached_crit);

    /* Removing the fault must bring it back to NORMAL. */
    sampler_set_offset(&smp, 0);
    for (i = 0; i < 500; i++) {
        st = alarm_update(&a, ema_update(&f, sampler_next(&smp)));
    }
    CHECK(st == ALARM_NORMAL);
}

int main(void)
{
    test_ringbuf();
    test_ringbuf_long_wrap();
    test_fmt_i32();
    test_fmt_parse();
    test_ema();
    test_alarm_transitions();
    test_alarm_jumps();
    test_alarm_no_chatter();
    test_stats();
    test_sampler();
    test_shell_basic();
    test_shell_edge_cases();
    test_pipeline_end_to_end();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
