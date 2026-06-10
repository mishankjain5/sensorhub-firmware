/*
 * SensorHub firmware entry point.
 *
 * Architecture: two interrupts produce data, one superloop consumes it.
 *
 *   SysTick ISR (100 Hz) --> g_samples ringbuf --+
 *                                                +--> main loop: filter ->
 *   UART0 RX ISR --------> g_rx ringbuf --------+    stats -> alarm FSM,
 *                                                     shell commands
 *
 * The ISRs only move bytes into ring buffers; all real work (filtering,
 * state machine, printing) happens in the main loop. That keeps interrupt
 * latency tiny and bounded — the golden rule of firmware design.
 */

#include <stdbool.h>
#include <stdint.h>

#include "../app/alarm.h"
#include "../app/ema.h"
#include "../app/fmt.h"
#include "../app/ringbuf.h"
#include "../app/sampler.h"
#include "../app/shell.h"
#include "../app/stats.h"
#include "systick.h"
#include "uart.h"

#define SYSCLK_HZ        12000000u   /* nominal QEMU LM3S6965 clock */
#define SAMPLE_HZ        100u
#define HEARTBEAT_EVERY  (2u * SAMPLE_HZ)    /* status line every 2 s */

#define DEFAULT_WARN     620
#define DEFAULT_CRIT     680
#define DEFAULT_HYST     10

/* All state is static (in .bss/.data) — the data path never touches a heap. */
static sampler_t g_sampler;
static ema_t     g_ema;
static stats_t   g_stats;
static alarm_t   g_alarm;
static shell_t   g_shell;

static rb_item_t g_sample_storage[64];
static ringbuf_t g_samples;              /* SysTick ISR -> main loop */
static rb_item_t g_rx_storage[128];   /* sized for pasted command bursts */
static ringbuf_t g_rx;                   /* UART RX ISR -> main loop */

static bool      g_quiet;
static int16_t   g_last_raw;
static int16_t   g_last_filt;
static uint32_t  g_nsamples;

/* --- interrupt handlers (names resolve the weak vectors in startup.c) --- */

void SysTick_Handler(void)
{
    ringbuf_push(&g_samples, sampler_next(&g_sampler));
}

/* UART0_Handler lives in uart.c and feeds g_rx. */

/* --- small print helpers (no printf on this image) --- */

static void print_int(const char *label, int32_t v)
{
    char buf[12];

    uart0_write(label);
    uart0_write(fmt_i32(v, buf));
}

static void print_reading(void)
{
    print_int("raw=", g_last_raw);
    print_int(" filt=", g_last_filt);
    uart0_write(" state=");
    uart0_write(alarm_state_name(g_alarm.state));
    uart0_write("\n");
}

/* --- shell commands --- */

static void cmd_help(void)
{
    uart0_write(
        "commands:\n"
        "  read            latest raw + filtered value\n"
        "  stats           min/max/mean since boot or reset\n"
        "  stats reset     restart the statistics\n"
        "  alarm           alarm state and thresholds\n"
        "  set warn <n>    set WARN threshold\n"
        "  set crit <n>    set CRIT threshold\n"
        "  sim offset <n>  inject a sensor offset (fault injection)\n"
        "  quiet           toggle the periodic status line\n");
}

static void cmd_stats(int argc, char **argv)
{
    if (argc == 2 && argv[1][0] == 'r') {
        stats_reset(&g_stats);
        uart0_write("stats reset\n");
        return;
    }
    print_int("count=", (int32_t)g_stats.count);
    print_int(" min=", g_stats.min);
    print_int(" max=", g_stats.max);
    print_int(" mean=", stats_mean(&g_stats));
    print_int(" lost_samples=", (int32_t)g_samples.dropped);
    print_int(" rx_overruns=", (int32_t)g_rx.dropped);
    uart0_write("\n");
}

static void cmd_alarm(void)
{
    uart0_write("state=");
    uart0_write(alarm_state_name(g_alarm.state));
    print_int(" warn=", g_alarm.warn);
    print_int(" crit=", g_alarm.crit);
    print_int(" hyst=", g_alarm.hyst);
    uart0_write("\n");
}

static bool str_eq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void cmd_handler(int argc, char **argv, void *user)
{
    (void)user;
    int32_t n;

    if (str_eq(argv[0], "help")) {
        cmd_help();
    } else if (str_eq(argv[0], "read")) {
        print_reading();
    } else if (str_eq(argv[0], "stats")) {
        cmd_stats(argc, argv);
    } else if (str_eq(argv[0], "alarm")) {
        cmd_alarm();
    } else if (str_eq(argv[0], "quiet")) {
        g_quiet = !g_quiet;
        uart0_write(g_quiet ? "status line off\n" : "status line on\n");
    } else if (str_eq(argv[0], "set") && argc == 3 && fmt_parse_i32(argv[2], &n)) {
        if (str_eq(argv[1], "warn")) {
            g_alarm.warn = (int16_t)n;
            cmd_alarm();
        } else if (str_eq(argv[1], "crit")) {
            g_alarm.crit = (int16_t)n;
            cmd_alarm();
        } else {
            uart0_write("usage: set warn|crit <n>\n");
        }
    } else if (str_eq(argv[0], "sim") && argc == 3 &&
               str_eq(argv[1], "offset") && fmt_parse_i32(argv[2], &n)) {
        sampler_set_offset(&g_sampler, (int16_t)n);
        print_int("offset=", n);
        uart0_write("\n");
    } else {
        uart0_write("unknown command (try 'help')\n");
    }
}

/* --- main loop --- */

static void wait_for_interrupt(void)
{
    /* Sleep until the next interrupt instead of spinning — on real
     * hardware this is the difference between days and months of battery. */
    __asm volatile ("wfi");
}

static void process_sample(int16_t raw)
{
    alarm_state_t before = g_alarm.state;
    alarm_state_t after;

    g_last_raw = raw;
    g_last_filt = ema_update(&g_ema, raw);
    stats_update(&g_stats, g_last_filt);
    after = alarm_update(&g_alarm, g_last_filt);
    g_nsamples++;

    if (after != before) {
        uart0_write("\n[ALARM] ");
        uart0_write(alarm_state_name(before));
        uart0_write(" -> ");
        uart0_write(alarm_state_name(after));
        print_int(" at filt=", g_last_filt);
        uart0_write("\n");
        shell_prompt(&g_shell);
    } else if (!g_quiet && (g_nsamples % HEARTBEAT_EVERY) == 0u) {
        print_int("\n[t=", (int32_t)(g_nsamples / SAMPLE_HZ));
        uart0_write("s] ");
        print_reading();
        shell_prompt(&g_shell);
    }
}

int main(void)
{
    rb_item_t item;

    ringbuf_init(&g_samples, g_sample_storage, 64);
    ringbuf_init(&g_rx, g_rx_storage, 128);
    sampler_init(&g_sampler, 0xC0FFEEu);
    ema_init(&g_ema);
    stats_reset(&g_stats);
    alarm_init(&g_alarm, DEFAULT_WARN, DEFAULT_CRIT, DEFAULT_HYST);
    shell_init(&g_shell, uart0_write, cmd_handler, 0);

    uart0_init(&g_rx);

    uart0_write("\nSensorHub v1.0 - bare-metal ARM Cortex-M3 (LM3S6965)\n");
    uart0_write("100 Hz sampling | EMA filter | alarm FSM | type 'help'\n\n");
    shell_prompt(&g_shell);

    systick_init(SYSCLK_HZ / SAMPLE_HZ);

    for (;;) {
        while (ringbuf_pop(&g_rx, &item)) {
            shell_input(&g_shell, (char)item);
        }
        while (ringbuf_pop(&g_samples, &item)) {
            process_sample(item);
        }
        wait_for_interrupt();
    }
}
