/* =============================================================================
 * SENG21213-OS :: Process Control Block (PCB) & Process Table
 * File   : kernel/process.h / process.c
 * Purpose: Defines the PCB and process-creation API for Lecture 9, extended
 *          in Lecture 10 with kernel-thread support (a thread is a PCB that
 *          shares its owner process's address space) and with the BLOCKED
 *          state + wait channel needed by mutexes / semaphores.
 *          Stacks are static arrays for now — the real PMM (dynamic frame
 *          allocation) arrives in Lecture 11.
 * ============================================================================*/
#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

/* 16 slots: Stage 1 uses 3 processes; Stage 2 adds up to ~7 kernel threads
 * (race coordinator, 4 racers, producer, consumer).                    */
#define MAX_PROCESSES     16
#define PROC_STACK_SIZE   4096
#define PROC_NAME_LEN     16

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,        /* L10: waiting on a mutex / semaphore (wait_chan) */
    PROC_TERMINATED
} proc_state_t;

typedef struct pcb {
    int           pid;               /* Also used as TID for threads        */
    proc_state_t  state;
    uint32_t      esp;              /* Saved stack pointer — see boot/switch.asm */
    char          name[PROC_NAME_LEN];
    struct pcb   *next;             /* Circular ready-queue link (scheduler.c) */

    /* --- Lecture 10 extensions --- */
    int           is_thread;        /* 0 = process, 1 = kernel thread       */
    int           owner_pid;        /* PID of owning process (threads only) */
    void         *wait_chan;        /* Address of mutex/sem we are blocked on */
} pcb_t;

void   process_init(void);
pcb_t *create_process(void (*entry)(void), const char *name);
void   process_kill(int pid);

/* Allocate a zeroed PCB + stack without building any initial stack frame.
 * The returned pcb's `esp` field holds the TOP of its fresh stack so the
 * caller (thread.c / process.c) can push the initial frame itself.      */
pcb_t *alloc_pcb(int is_thread, int owner_pid, const char *name);

/* Iterate the process table for the `ps` / `threads` shell commands. */
pcb_t *process_table_get(int index);
int    process_table_max(void);

#endif /* PROCESS_H */
