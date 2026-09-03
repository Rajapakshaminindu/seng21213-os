/* =============================================================================
 * SENG21213-OS :: Counting semaphore
 * File   : kernel/semaphore.h / semaphore.c
 * Purpose: Lecture 10 §5 — Dijkstra's counting semaphore with blocking
 *          sem_wait() (P) and sem_signal() (V). Used by the Stage 2
 *          bounded-buffer producer/consumer demo (empty / full / mutex).
 * ============================================================================*/
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

typedef struct semaphore {
    volatile int count;       /* >= 0; number of available resources */
    const char  *name;        /* For debug listings */
} sem_t;

void sem_init(sem_t *s, int count, const char *name);
void sem_wait(sem_t *s);      /* P(): decrement or sleep (blocking) */
void sem_signal(sem_t *s);    /* V(): increment and wake one waiter */

#endif /* SEMAPHORE_H */
