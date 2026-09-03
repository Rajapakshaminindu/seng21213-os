/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.h / scheduler.c
 * Purpose: Maintains a circular ready-queue of PCBs and switches between
 *          them on every PIT tick (100 Hz).                (Lecture 9 §4)
 * ============================================================================*/
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void   scheduler_init(void);
void   scheduler_add(pcb_t *p);

/* Switches the CPU into the first ready process. Never returns. */
void   scheduler_start(void) __attribute__((noreturn));

/* Called from kernel/pit.c's irq0_c_handler on every timer tick. */
void   scheduler_tick(void);

pcb_t *scheduler_current(void);

#endif /* SCHEDULER_H */
