#include <stdint.h>
#include "uart.h"

/*
 * Memory-mapped I/O: each "register" below is just a fixed address that the
 * bus routes to the UART peripheral instead of RAM. `volatile` tells the
 * compiler every read/write has a side effect and must actually happen,
 * in order — without it the optimizer would happily delete the busy-wait
 * loop in uart0_putc().
 */
#define REG32(addr) (*(volatile uint32_t *)(addr))

#define UART0_BASE   0x4000C000u
#define UART0_DR     REG32(UART0_BASE + 0x000u)  /* data                     */
#define UART0_FR     REG32(UART0_BASE + 0x018u)  /* flags                    */
#define UART0_IBRD   REG32(UART0_BASE + 0x024u)  /* baud divisor, integer    */
#define UART0_FBRD   REG32(UART0_BASE + 0x028u)  /* baud divisor, fraction   */
#define UART0_LCRH   REG32(UART0_BASE + 0x02Cu)  /* line control             */
#define UART0_CTL    REG32(UART0_BASE + 0x030u)  /* enable bits              */
#define UART0_IM     REG32(UART0_BASE + 0x038u)  /* interrupt mask           */
#define UART0_ICR    REG32(UART0_BASE + 0x044u)  /* interrupt clear          */

#define FR_TXFF      (1u << 5)                   /* TX FIFO full             */
#define FR_RXFE      (1u << 4)                   /* RX FIFO empty            */
#define IM_RXIM      (1u << 4)                   /* RX interrupt enable      */
#define CTL_UARTEN   (1u << 0)
#define CTL_TXE      (1u << 8)
#define CTL_RXE      (1u << 9)
#define LCRH_WLEN8   (3u << 5)                   /* 8 data bits              */

/* System control: peripherals are clock-gated off until enabled. */
#define SYSCTL_RCGC1 REG32(0x400FE104u)
#define SYSCTL_RCGC2 REG32(0x400FE108u)
#define RCGC1_UART0  (1u << 0)
#define RCGC2_GPIOA  (1u << 0)                   /* UART0 pins live on port A */

/* NVIC: the Cortex-M interrupt controller. Writing bit N of EN0 enables
 * peripheral interrupt N; UART0 is interrupt #5 on this chip. */
#define NVIC_EN0     REG32(0xE000E100u)
#define UART0_IRQ    5u

static ringbuf_t *rx_ringbuf;

void uart0_init(ringbuf_t *rx_buf)
{
    rx_ringbuf = rx_buf;

    SYSCTL_RCGC1 |= RCGC1_UART0;     /* clock the UART...                  */
    SYSCTL_RCGC2 |= RCGC2_GPIOA;     /* ...and the GPIO port with its pins */

    UART0_CTL = 0;                   /* disable while configuring          */

    /* 115200 baud assuming a 12 MHz system clock:
     * divisor = 12e6 / (16 * 115200) = 6.510 -> IBRD=6, FBRD=round(.510*64)=33.
     * (QEMU ignores baud timing, but real hardware would not.) */
    UART0_IBRD = 6;
    UART0_FBRD = 33;
    UART0_LCRH = LCRH_WLEN8;         /* 8 data bits, no parity, 1 stop, no FIFO */
    UART0_IM   = IM_RXIM;            /* interrupt on every received byte   */
    UART0_CTL  = CTL_UARTEN | CTL_TXE | CTL_RXE;

    NVIC_EN0 = (1u << UART0_IRQ);
}

void uart0_putc(char c)
{
    while (UART0_FR & FR_TXFF) {
        /* busy-wait for space — volatile makes this loop real */
    }
    UART0_DR = (uint32_t)(uint8_t)c;
}

void uart0_write(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart0_putc('\r');
        }
        uart0_putc(*s++);
    }
}

/* Interrupt handler — the name matches the weak symbol in startup.c, so
 * the linker drops this into vector table slot 21 automatically.
 * Keep it short: grab the bytes, hand them to the ring buffer, get out. */
void UART0_Handler(void)
{
    while ((UART0_FR & FR_RXFE) == 0) {
        ringbuf_push(rx_ringbuf, (rb_item_t)(UART0_DR & 0xFFu));
    }
    UART0_ICR = IM_RXIM;             /* acknowledge the interrupt */
}
