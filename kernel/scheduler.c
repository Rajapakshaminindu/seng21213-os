/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler implementation
 * File   : kernel/scheduler.c
 *
 * DESIGN NOTE (Lecture 9 §4)
 *   Every PCB's `esp` field either points at:
 *     (a) a freshly-built stack from create_process() — [edi][esi][ebx][ebp][entry]
 *     (b) a stack saved mid-way through a previous switch_context() call,
 *         which itself is nested inside an IRQ0 interrupt that is still
 *         "in flight" for that process.
 *   Either way, switch_context()'s `pop edi/esi/ebx/ebp; ret` unwinds
 *   correctly: case (a) jumps straight into entry(), case (b) resumes the
 *   suspended call chain all the way back up through scheduler_tick() ->
 *   irq0_c_handler() -> the asm ISR's `popad; iretd`, which restores that
 *   process's full register set and resumes it exactly where it left off.
 * ============================================================================*/
#include "scheduler.h"

/* Defined in boot/switch.asm */
extern void switch_context(uint32_t *old_esp_store, uint32_t new_esp);

static pcb_t *ready_head = 0;   /* Circular linked list of READY/RUNNING PCBs */
static pcb_t *current    = 0;

void scheduler_init(void) {
    ready_head = 0;
    current    = 0;
}

void scheduler_add(pcb_t *p) {
    if (!p) return;
    p->state = PROC_READY;

    if (!ready_head) {
        ready_head = p;
        p->next    = p;
        return;
    }

    pcb_t *tail = ready_head;
    while (tail->next != ready_head) tail = tail->next;
    tail->next  = p;
    p->next     = ready_head;
}

pcb_t *scheduler_current(void) {
    return current;
}

void scheduler_start(void) {
    current = ready_head;
    current->state = PROC_RUNNING;

    /* We never resume the original kernel_main() stack, but switch_context
     * still needs somewhere to record it. */
    static uint32_t unused_boot_esp;
    switch_context(&unused_boot_esp, current->esp);

    /* Unreachable — kept only to satisfy __attribute__((noreturn)). */
    for (;;) { __asm__ __volatile__("hlt"); }
}

void scheduler_tick(void) {
    if (!current) return;

    pcb_t *prev = current;
    pcb_t *next = prev->next;

    /* Skip terminated processes; bail out if nobody else is runnable. */
    for (int guard = 0; guard < MAX_PROCESSES && next->state == PROC_TERMINATED; guard++) {
        next = next->next;
    }
    if (next == prev || next->state == PROC_TERMINATED) {
        return; /* Nothing to switch to this tick */
    }

    prev->state = PROC_READY;
    next->state = PROC_RUNNING;
    current     = next;

    switch_context(&prev->esp, next->esp);
    /* When `prev` is resumed again later, execution continues right here. */
}
