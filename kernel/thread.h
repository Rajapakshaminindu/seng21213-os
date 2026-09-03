/* =============================================================================
 * SENG21213-OS :: Kernel threads
 * File   : kernel/thread.h / thread.c
 * Purpose: Lecture 10 §2 — kernel threads sharing their owner process's
 *          address space. A thread is a schedulable context (PCB entry with
 *          is_thread = 1) with its OWN stack and register state but no own
 *          address space; it is scheduled by the same round-robin scheduler
 *          as processes. thread_create(fn, arg) passes one argument to the
 *          thread function, exactly like the lecture API.
 * ============================================================================*/
#ifndef THREAD_H
#define THREAD_H

#include "process.h"

/* Create a kernel thread owned by `owner_pid` running fn(arg).
 * Returns the new TID (>0) or -1 if the table is full. */
int  thread_create_in(int owner_pid, void (*fn)(void *), void *arg,
                      const char *name);

/* Same, but the owner is the calling context's process (threads inherit
 * their creator's owner, so a thread created from a thread belongs to the
 * same process). */
int  thread_create(void (*fn)(void *), void *arg, const char *name);

/* Terminate the calling thread forever (never returns). */
void thread_exit(void) __attribute__((noreturn));

#endif /* THREAD_H */
