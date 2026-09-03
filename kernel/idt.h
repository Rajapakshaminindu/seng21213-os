/* =============================================================================
 * SENG21213-OS :: Interrupt Descriptor Table (IDT)
 * File   : kernel/idt.h / idt.c
 * Purpose: Minimal IDT sufficient to route IRQ0 (PIT timer) to our
 *          assembly ISR stub so the round-robin scheduler can run.
 *          (Lecture 9 §2 — Interrupt-driven scheduling)
 * ============================================================================*/
#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

#endif /* IDT_H */
