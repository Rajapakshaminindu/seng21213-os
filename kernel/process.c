/* =============================================================================
 * SENG21213-OS :: Process management implementation
 * File   : kernel/process.c
 * ============================================================================*/
#include "process.h"

static pcb_t   proc_table[MAX_PROCESSES];
static uint8_t proc_stacks[MAX_PROCESSES][PROC_STACK_SIZE];
static int     next_pid = 1;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].pid   = 0;
        proc_table[i].next  = 0;
    }
    next_pid = 1;
}

/* Build a fresh stack frame matching exactly what boot/switch.asm's
 * `pop edi; pop esi; pop ebx; pop ebp; ret` sequence expects, so the
 * very first switch into this process jumps straight into entry(). */
pcb_t *create_process(void (*entry)(void), const char *name) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_UNUSED) continue;

        pcb_t    *p  = &proc_table[i];
        uint32_t *sp = (uint32_t *)(proc_stacks[i] + PROC_STACK_SIZE);

        *(--sp) = (uint32_t)entry;  /* return address for switch_context's ret */
        *(--sp) = 0;                /* edi */
        *(--sp) = 0;                /* esi */
        *(--sp) = 0;                /* ebx */
        *(--sp) = 0;                /* ebp */

        p->esp   = (uint32_t)sp;
        p->pid   = next_pid++;
        p->state = PROC_READY;
        p->next  = 0;

        int j = 0;
        while (name[j] && j < PROC_NAME_LEN - 1) { p->name[j] = name[j]; j++; }
        p->name[j] = '\0';

        return p;
    }
    return 0; /* Process table full */
}

void process_kill(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_UNUSED && proc_table[i].pid == pid) {
            proc_table[i].state = PROC_TERMINATED;
            return;
        }
    }
}

pcb_t *process_table_get(int index) {
    if (index < 0 || index >= MAX_PROCESSES) return 0;
    if (proc_table[index].state == PROC_UNUSED) return 0;
    return &proc_table[index];
}

int process_table_max(void) {
    return MAX_PROCESSES;
}
