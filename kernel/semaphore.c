/* =============================================================================
 * SENG21213-OS :: Counting semaphore implementation
 * File   : kernel/semaphore.c
 *
 * Same atomicity scheme as mutex.c (Lecture 10 §5): the decrement-or-sleep
 * decision is made with interrupts disabled and stays disabled across the
 * sleep, so a sem_signal() can never slip into the gap between "count == 0"
 * and "waiter registered on the channel" (no lost wakeups).
 * ============================================================================*/
#include "semaphore.h"
#include "scheduler.h"

void sem_init(sem_t *s, int count, const char *name) {
    s->count = count;
    s->name  = name;
}

void sem_wait(sem_t *s) {
    __asm__ volatile("cli");
    for (;;) {
        if (s->count > 0) {               /* Resource available: take it... */
            s->count--;
            __asm__ volatile("sti");      /* ...and re-enable interrupts. */
            return;
        }
        /* None left: sleep (returns with IF=0), re-test in this cli loop. */
        scheduler_block_chan(s);
    }
}

void sem_signal(sem_t *s) {
    __asm__ volatile("cli");
    s->count++;
    scheduler_wake_chan(s, 1);            /* One waiter re-tests and takes it */
    __asm__ volatile("sti");
}
