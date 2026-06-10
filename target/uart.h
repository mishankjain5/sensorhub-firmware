#ifndef SENSORHUB_TARGET_UART_H
#define SENSORHUB_TARGET_UART_H

#include "../app/ringbuf.h"

/*
 * UART0 driver for the LM3S6965.
 * TX is blocking (we wait for FIFO space); RX is interrupt-driven into a
 * caller-provided ring buffer, which the main loop drains at its leisure.
 */

/* Initialize UART0 at 115200 8N1 and enable the receive interrupt.
 * Received bytes are pushed into rx_buf from the interrupt handler. */
void uart0_init(ringbuf_t *rx_buf);

void uart0_putc(char c);

/* Write a NUL-terminated string, translating '\n' to "\r\n" for terminals. */
void uart0_write(const char *s);

#endif
