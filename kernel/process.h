/* =============================================================================
 * SENG21213-OS :: Process Control Block (PCB) & Process Table
 * File   : kernel/process.h / process.c
 * Purpose: Defines the PCB and process-creation API for Lecture 9.
 *          Stacks are static arrays for now — the real PMM (dynamic frame
 *          allocation) arrives in Lecture 11.
 * ============================================================================*/
#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES     8
#define PROC_STACK_SIZE   4096
#define PROC_NAME_LEN     16

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_TERMINATED
} proc_state_t;

typedef struct pcb {
    int           pid;
    proc_state_t  state;
    uint32_t      esp;              /* Saved stack pointer — see boot/switch.asm */
    char          name[PROC_NAME_LEN];
    struct pcb   *next;             /* Circular ready-queue link (scheduler.c) */
} pcb_t;

void   process_init(void);
pcb_t *create_process(void (*entry)(void), const char *name);
void   process_kill(int pid);

/* Iterate the process table for the `ps` shell command. */
pcb_t *process_table_get(int index);
int    process_table_max(void);

#endif /* PROCESS_H */
