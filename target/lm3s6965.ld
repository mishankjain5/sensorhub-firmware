/*
 * Linker script for the TI Stellaris LM3S6965 (ARM Cortex-M3).
 *
 * This file answers the question "where does every byte of my program
 * live?" — something a desktop OS normally hides from you.
 *
 *   FLASH (256 KiB at 0x00000000): code, constants, and the initial
 *         values of initialized variables. Survives power-off.
 *   SRAM  ( 64 KiB at 0x20000000): variables, stack. Lost at power-off.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

/* Initial stack pointer: the very top of SRAM (the stack grows down). */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* The Cortex-M3 reads address 0 at reset expecting the vector table,
     * so it must be the first thing in flash. */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Initialized globals: they LIVE in SRAM but their initial values are
     * STORED in flash. startup.c copies them across at reset. */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    _sidata = LOADADDR(.data);   /* where the flash copy of .data starts */

    /* Zero-initialized globals: no flash copy needed, startup.c just
     * fills the region with zeroes. */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM
}
