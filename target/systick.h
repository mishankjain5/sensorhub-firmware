#ifndef SENSORHUB_TARGET_SYSTICK_H
#define SENSORHUB_TARGET_SYSTICK_H

#include <stdint.h>

/* Start the Cortex-M SysTick timer firing the SysTick interrupt every
 * `ticks` CPU clock cycles. The handler itself is SysTick_Handler()
 * (defined in main.c, wired through the vector table in startup.c). */
void systick_init(uint32_t ticks);

#endif
