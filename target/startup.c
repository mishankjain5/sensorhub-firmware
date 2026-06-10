/*
 * Bare-metal startup for the LM3S6965 (Cortex-M3).
 *
 * There is no operating system here. When power arrives, the CPU reads two
 * words from address 0: the initial stack pointer and the address of
 * Reset_Handler. Everything that a C program takes for granted — variables
 * having their initial values — is done by hand below, before main() runs.
 */

#include <stdint.h>

/* Symbols defined by the linker script (addresses, not variables). */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int main(void);

void Reset_Handler(void)
{
    uint32_t *src;
    uint32_t *dst;

    /* Copy initialized globals (.data) from flash to SRAM. */
    src = &_sidata;
    for (dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }

    /* Zero out uninitialized globals (.bss) — C guarantees they start at 0. */
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }

    main();

    /* main() should never return in firmware; trap it if it does. */
    for (;;) {
    }
}

/* Any interrupt we did not write a handler for lands here. Spinning in
 * place keeps the failure visible to a debugger instead of running off
 * into random memory. */
void Default_Handler(void)
{
    for (;;) {
    }
}

/* Weak aliases: each handler defaults to Default_Handler until a real
 * function with the same name is defined elsewhere (see main.c). */
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)__attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UART0_Handler(void)     __attribute__((weak, alias("Default_Handler")));

/*
 * The vector table. Entry 0 is the initial stack pointer; the rest are
 * function addresses the hardware jumps to. Slots 0-15 are fixed by ARM;
 * from slot 16 on they are the chip vendor's peripheral interrupts
 * (on the LM3S6965, UART0 is peripheral interrupt #5 = slot 21).
 */
__attribute__((section(".isr_vector"), used))
static void (*const vector_table[])(void) = {
    (void (*)(void))&_estack,   /*  0: initial stack pointer            */
    Reset_Handler,              /*  1: reset                            */
    NMI_Handler,                /*  2: non-maskable interrupt           */
    HardFault_Handler,          /*  3: all classes of fault             */
    MemManage_Handler,          /*  4: memory protection violation      */
    BusFault_Handler,           /*  5: bus error                        */
    UsageFault_Handler,         /*  6: undefined instruction etc.       */
    0, 0, 0, 0,                 /*  7-10: reserved                      */
    SVC_Handler,                /* 11: supervisor call                  */
    0,                          /* 12: debug monitor (reserved here)    */
    0,                          /* 13: reserved                         */
    PendSV_Handler,             /* 14: pendable service                 */
    SysTick_Handler,            /* 15: system tick timer                */
    Default_Handler,            /* 16: IRQ 0  GPIO port A               */
    Default_Handler,            /* 17: IRQ 1  GPIO port B               */
    Default_Handler,            /* 18: IRQ 2  GPIO port C               */
    Default_Handler,            /* 19: IRQ 3  GPIO port D               */
    Default_Handler,            /* 20: IRQ 4  GPIO port E               */
    UART0_Handler,              /* 21: IRQ 5  UART0                     */
};
