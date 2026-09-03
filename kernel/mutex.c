/* =============================================================================
 * SENG21213-OS :: Blocking mutex implementation
 * File   : kernel/mutex.c
 *
 * The test-and-set of `locked` is made atomic with respect to the 100 Hz
 * timer interrupt by holding cli across the whole check-or-sleep decision
 * — on a single-CPU kernel that is the entire story (Lecture 10 §4).
 * If the lock is held, the caller sleeps on the mutex itself (used as the
 * wait channel) and is woken again by mutex_unlock(); because interrupts
 * stay off across check+sleep, no unlock can slip into the gap and lose
 * the wakeup (the classic lost-wakeup race).
 * ============================================================================*/
#include "mutex.h"
#include "scheduler.h"

void mutex_init(mutex_t *m, const char *name) {
    m->locked = 0;
    m->name   = name;
}

void mutex_lock(mutex_t *m) {
    __asm__ volatile("cli");
    for (;;) {
        if (!m->locked) {                 /* Free: take it atomically... */
            m->locked = 1;
            __asm__ volatile("sti");      /* ...and re-enable interrupts. */
            return;
        }
        /* Held: sleep (returns with IF=0), then re-test in this cli loop. */
        scheduler_block_chan(m);
    }
}

void mutex_unlock(mutex_t *m) {
    __asm__ volatile("cli");
    m->locked = 0;
    scheduler_wake_chan(m, 1);            /* One waiter re-tests and takes it */
    __asm__ volatile("sti");
}
