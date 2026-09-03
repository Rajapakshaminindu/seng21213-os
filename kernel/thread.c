/* =============================================================================
 * SENG21213-OS :: Kernel threads implementation
 * File   : kernel/thread.c
 *
 * A thread's initial stack is built so that boot/switch.asm's
 *   `pop edi; pop esi; pop ebx; pop ebp; ret`
 * sequence lands in thread_trampoline() with a valid cdecl argument frame:
 *
 *   high address
 *     [ arg            ]   <- trampoline's 2nd argument  ([esp+8] at entry)
 *     [ fn             ]   <- trampoline's 1st argument  ([esp+4] at entry)
 *     [ thread_exit    ]   <- fake return address        ([esp]   at entry)
 *     [ trampoline     ]   <- popped by switch_context's `ret`
 *     [ 0 (ebp)        ]
 *     [ 0 (ebx)        ]
 *     [ 0 (esi)        ]
 *     [ 0 (edi)        ]   <- saved esp points here
 *   low address
 * ============================================================================*/
#include "thread.h"
#include "scheduler.h"

/* First code to run on a brand-new thread's stack. The `sti` is mandatory:
 * the very first switch into a thread happens inside the IRQ0 ISR (IF=0)
 * and switch_context() does not save/restore EFLAGS — see the EFLAGS note
 * in kernel/scheduler.c. On later resumes this is a harmless no-op. */
static void thread_trampoline(void (*fn)(void *), void *arg) {
    __asm__ __volatile__("sti");
    fn(arg);
    thread_exit();            /* Threads that simply return are reaped */
}

int thread_create_in(int owner_pid, void (*fn)(void *), void *arg,
                     const char *name) {
    pcb_t *p = alloc_pcb(1, owner_pid, name);
    if (!p) return -1;

    uint32_t *sp = (uint32_t *)p->esp;
    *(--sp) = (uint32_t)arg;               /* [esp+8] at trampoline entry */
    *(--sp) = (uint32_t)fn;                /* [esp+4] at trampoline entry */
    *(--sp) = (uint32_t)thread_exit;       /* [esp]   fake return address */
    *(--sp) = (uint32_t)thread_trampoline; /* popped by switch_context ret */
    *(--sp) = 0;                           /* ebp */
    *(--sp) = 0;                           /* ebx */
    *(--sp) = 0;                           /* esi */
    *(--sp) = 0;                           /* edi */
    p->esp  = (uint32_t)sp;

    scheduler_add(p);
    return p->pid;
}

int thread_create(void (*fn)(void *), void *arg, const char *name) {
    pcb_t *cur = scheduler_current();
    int owner = 0;
    if (cur) owner = cur->is_thread ? cur->owner_pid : cur->pid;
    return thread_create_in(owner, fn, arg, name);
}

void thread_exit(void) {
    scheduler_exit_current();
    /* Unreachable */
    for (;;) { __asm__ __volatile__("hlt"); }
}
