#include "systick.h"

/* SysTick is part of the Cortex-M core itself (not the chip vendor's
 * peripherals), so these addresses are the same on every Cortex-M MCU. */
#define REG32(addr) (*(volatile uint32_t *)(addr))

#define SYST_CSR  REG32(0xE000E010u)   /* control and status     */
#define SYST_RVR  REG32(0xE000E014u)   /* reload value           */
#define SYST_CVR  REG32(0xE000E018u)   /* current value          */

#define CSR_ENABLE    (1u << 0)        /* run the counter        */
#define CSR_TICKINT   (1u << 1)        /* fire the interrupt     */
#define CSR_CLKSOURCE (1u << 2)        /* clock from the CPU     */

void systick_init(uint32_t ticks)
{
    SYST_RVR = ticks - 1u;             /* counts N-1 .. 0, then reloads */
    SYST_CVR = 0;                      /* writing clears the counter    */
    SYST_CSR = CSR_ENABLE | CSR_TICKINT | CSR_CLKSOURCE;
}
