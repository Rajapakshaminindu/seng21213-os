/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler implementation
 * File   : kernel/scheduler.c
 *
 * DESIGN NOTE (Lecture 9 §4)
 *   Every PCB's `esp` field either points at:
 *     (a) a freshly-built stack from create_process()/thread_create_in()
 *     (b) a stack saved mid-way through a previous switch_context() call,
 *         which itself is nested inside an IRQ0 interrupt (or inside a
 *         voluntary scheduler_block_chan() call) that is still "in flight"
 *         for that context.
 *   Either way, switch_context()'s `pop edi/esi/ebx/ebp; ret` unwinds
 *   correctly: case (a) jumps straight into the entry function, case (b)
 *   resumes the suspended call chain all the way back up through
 *   scheduler_tick() -> irq0_c_handler() -> the asm ISR's `popad; iretd`
 *   (or back out of scheduler_block_chan()), which restores that context's
 *   full register set and resumes it exactly where it left off.
 *
 * EFLAGS / IF NOTE (Lecture 10 pitfall, see also kernel/kernel.c)
 *   switch_context() saves only 4 callee-saved registers — NOT EFLAGS.
 *   A context switched in from inside the IRQ0 ISR therefore runs with
 *   IF=0 until it either reaches a pending `iretd` or executes an explicit
 *   `sti`. Every voluntary switch path below (block / exit) therefore ends
 *   with an `sti` that runs on the RESUME side, and new contexts `sti` in
 *   their trampolines. Interrupt-gate resumes fix IF via their own iretd.
 * ============================================================================*/
#include "scheduler.h"

/* Defined in boot/switch.asm */
extern void switch_context(uint32_t *old_esp_store, uint32_t new_esp);

static pcb_t *ready_head = 0;   /* Circular linked list of all scheduled PCBs */
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

/* Walk the circular list starting after `from` and return the first PCB in
 * PROC_READY state, or 0 if nobody but (possibly) `from` is runnable. */
static pcb_t *pick_next_ready(pcb_t *from) {
    pcb_t *p = from->next;
    for (int guard = 0; guard < MAX_PROCESSES; guard++) {
        if (p->state == PROC_READY) return p;
        p = p->next;
    }
    return 0;
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
    pcb_t *next = pick_next_ready(prev);

    /* Nobody else runnable (everyone else BLOCKED/TERMINATED, or we are the
     * only context): keep running `prev`, nothing to switch to this tick. */
    if (!next || next == prev) return;

    if (prev->state == PROC_RUNNING) prev->state = PROC_READY;
    next->state = PROC_RUNNING;
    current     = next;

    switch_context(&prev->esp, next->esp);
    /* When `prev` is resumed again later, execution continues right here. */
}

/* --- Lecture 10: voluntary blocking (mutex / semaphore sleep) ------------*/

void scheduler_block_chan(void *chan) {
    /* PRECONDITION: called with interrupts DISABLED, and the caller keeps
     * them disabled after we return (it re-tests its condition atomically
     * in a cli loop). Keeping cli held from the caller's condition check
     * through this state change is what closes the lost-wakeup race:
     * no unlock/sem_signal can slip in between "condition false" and
     * "wait_chan registered".                                    (L10 §4) */
    pcb_t *prev = current;
    prev->wait_chan = chan;
    prev->state     = PROC_BLOCKED;

    pcb_t *next = pick_next_ready(prev);
    if (!next) {
        /* Pathological: nothing else runnable — the lock/semaphore can
         * never be released anyway (its holder is not runnable), so this
         * is a true deadlock; spin with interrupts off. Cannot happen in
         * our demos: the shell process is always runnable. */
        prev->state     = PROC_READY;
        prev->wait_chan = 0;
        return;
    }

    next->state = PROC_RUNNING;
    current     = next;

    switch_context(&prev->esp, next->esp);

    /* RESUME SIDE (someone woke us): interrupts are STILL DISABLED here —
     * the switch back into us may have happened with IF=0 from inside the
     * IRQ0 ISR, and switch_context() does not save/restore EFLAGS. The
     * caller re-tests its condition under cli and re-enables interrupts
     * itself once it stops blocking. See the EFLAGS note at the top. */
}

int scheduler_wake_chan(void *chan, int wake_all) {
    int woke = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_get(i);
        if (p && p->state == PROC_BLOCKED && p->wait_chan == chan) {
            p->state     = PROC_READY;
            p->wait_chan = 0;
            woke++;
            if (!wake_all) break;
        }
    }
    return woke;
}

void scheduler_exit_current(void) {
    __asm__ volatile("cli");

    pcb_t *prev = current;
    prev->state     = PROC_TERMINATED;
    prev->wait_chan = 0;

    pcb_t *next = pick_next_ready(prev);
    if (!next) {
        /* Every context exited — nothing left to run. */
        for (;;) { __asm__ volatile("hlt"); }
    }

    next->state = PROC_RUNNING;
    current     = next;

    switch_context(&prev->esp, next->esp);

    /* Never resumed. */
    for (;;) { __asm__ __volatile__("hlt"); }
}
