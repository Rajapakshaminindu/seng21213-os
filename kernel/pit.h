/* =============================================================================
 * SENG21213-OS :: i8253 Programmable Interval Timer (PIT) driver
 * File   : kernel/pit.h / pit.c
 * Purpose: Programs PIT channel 0 to fire IRQ0 at 100 Hz (a 10 ms tick),
 *          the heartbeat that drives our round-robin scheduler.
 *          (Lecture 9 §3)
 * ============================================================================*/
#ifndef PIT_H
#define PIT_H

#include "../include/types.h"

/* Number of IRQ0 ticks since boot. Demo processes poll this to decide
 * when to print — see kernel/kernel.c demo_task_a/demo_task_b. */
extern volatile uint32_t pit_ticks;

void pit_init(uint32_t frequency_hz);

/* Called from kernel/isr_irq0.asm on every timer interrupt. */
void irq0_c_handler(void);

#endif /* PIT_H */
