#ifndef SENSORHUB_SHELL_H
#define SENSORHUB_SHELL_H

#include <stdint.h>

/*
 * Minimal line-oriented command shell.
 *
 * The shell knows nothing about UARTs or commands: bytes come in through
 * shell_input(), output goes out through a caller-supplied write function,
 * and completed lines are tokenized and handed to a caller-supplied
 * handler. That keeps this module 100% portable and unit-testable on the
 * host — the firmware target wires it to the real UART in main().
 */

#define SHELL_LINE_MAX 48
#define SHELL_MAX_ARGS 4

typedef void (*shell_out_fn)(const char *s);
typedef void (*shell_cmd_fn)(int argc, char **argv, void *user);

typedef struct {
    char line[SHELL_LINE_MAX];
    uint8_t len;
    shell_out_fn out;
    shell_cmd_fn handler;
    void *user;
} shell_t;

void shell_init(shell_t *sh, shell_out_fn out, shell_cmd_fn handler, void *user);

/* Feed one received byte. Printable bytes are echoed and buffered,
 * backspace edits, CR/LF terminates the line and runs the handler. */
void shell_input(shell_t *sh, char c);

void shell_prompt(shell_t *sh);

#endif
