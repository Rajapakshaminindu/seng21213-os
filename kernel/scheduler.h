/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.h / scheduler.c
 * Purpose: Maintains a circular ready-queue of PCBs and switches between
 *          them on every PIT tick (100 Hz).                (Lecture 9 §4)
 *          Lecture 10 adds the blocking primitives that mutexes and
 *          semaphores sleep on: scheduler_block_chan() / wake / exit.
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

/* --- Lecture 10: blocking / waking (used by mutex.c & semaphore.c) ---
 *
 * scheduler_block_chan(): puts the CALLING context to sleep on `chan`
 *   (its pcb->wait_chan = chan, state = BLOCKED) and immediately switches
 *   to the next READY context. MUST be called with interrupts disabled,
 *   from thread/process context (never from an ISR); on wake-up it returns
 *   with interrupts STILL disabled so the caller can re-test its condition
 *   atomically (this is what closes the lost-wakeup race, L10 §4).
 * scheduler_wake_chan(): marks BLOCKED waiters of `chan` READY again.
 *   wake_all != 0 wakes every waiter, otherwise just one.
 * scheduler_exit_current(): terminates the calling context forever.     */
void scheduler_block_chan(void *chan);
int  scheduler_wake_chan(void *chan, int wake_all);
void scheduler_exit_current(void) __attribute__((noreturn));

#endif /* SCHEDULER_H */
