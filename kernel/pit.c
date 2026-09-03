/* =============================================================================
 * SENG21213-OS :: PIT driver implementation
 * File   : kernel/pit.c
 * ============================================================================*/
#include "pit.h"
#include "pic.h"
#include "scheduler.h"
#include "../include/io.h"

#define PIT_CHANNEL0   0x40
#define PIT_COMMAND    0x43
#define PIT_BASE_FREQ  1193182u   /* Hz — the PIT's fixed input clock */

volatile uint32_t pit_ticks = 0;

void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    /* Channel 0, access mode = lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_COMMAND, 0x36);

    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

/* Called from the asm ISR trampoline (kernel/isr_irq0.asm) on every
 * IRQ0 (timer) interrupt. Runs with interrupts disabled (interrupt gate). */
void irq0_c_handler(void) {
    pit_ticks++;
    pic_send_eoi(0);

    /* May context-switch inside here via switch_context(); if it does,
     * execution of THIS call resumes later, when this process is switched
     * back in — see kernel/scheduler.c for the full explanation. */
    scheduler_tick();
}
