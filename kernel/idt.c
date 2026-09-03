/* =============================================================================
 * SENG21213-OS :: IDT implementation
 * File   : kernel/idt.c
 *
 * The bootloader's GDT (boot/boot.asm) defines CODE_SEG = 0x08. We reuse
 * that selector for every gate here since Stage 1 runs entirely in Ring 0
 * with no privilege separation.
 * ============================================================================*/
#include "idt.h"
#include "pic.h"

#define IDT_ENTRIES 256

/* One IDT gate descriptor (8 bytes on x86-32) */
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* Defined in kernel/isr_irq0.asm — the low-level ISR trampoline for IRQ0 */
extern void irq0_stub(void);
/* Also in isr_irq0.asm — safe EOI-and-return catch-all for unused IRQs */
extern void irq_ignore_stub(void);

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector   = selector;
    idt[num].zero       = 0;
    idt[num].flags      = flags;
}

static inline void idt_load(void) {
    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}

void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
    }

    /* 0x08 = kernel code selector (see boot/boot.asm CODE_SEG).
     * 0x8E = Present | Ring0 | 32-bit interrupt gate (IF cleared on entry). */
    idt_set_gate(0x20, (uint32_t)irq0_stub, 0x08, 0x8E);

    /* Defense-in-depth: point every other (masked) IRQ vector 0x21..0x2F at a
     * safe stub that just EOI's and returns. Without a gate here, an unexpected
     * device interrupt would dereference a null descriptor -> #GP -> #DF ->
     * triple fault -> reboot. See kernel/isr_irq0.asm. */
    for (int v = 0x21; v <= 0x2F; v++) {
        idt_set_gate((uint8_t)v, (uint32_t)irq_ignore_stub, 0x08, 0x8E);
    }

    idt_load();
}
