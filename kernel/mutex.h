/* =============================================================================
 * SENG21213-OS :: Blocking mutex
 * File   : kernel/mutex.h / mutex.c
 * Purpose: Lecture 10 §4 — a spin-free, blocking mutual-exclusion lock.
 *          mutex_lock() sleeps the calling thread on the mutex's wait
 *          channel (scheduler_block_chan) instead of burning CPU, and
 *          mutex_unlock() hands the lock to a waiter.
 * ============================================================================*/
#ifndef MUTEX_H
#define MUTEX_H

typedef struct mutex {
    volatile int locked;      /* 0 = free, 1 = held */
    const char  *name;        /* For debug / `threads` style listings */
} mutex_t;

void mutex_init(mutex_t *m, const char *name);
void mutex_lock(mutex_t *m);    /* Blocking (L10 §4) */
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
