/* =============================================================================
 * SENG21213-OS :: 8259 PIC driver implementation
 * File   : kernel/pic.c
 * ============================================================================*/
#include "pic.h"
#include "../include/io.h"

void pic_remap(void) {
    /* ICW1: begin initialisation, expect ICW4 */
    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    /* ICW2: vector offsets — IRQ0..7 -> 0x20..0x27, IRQ8..15 -> 0x28..0x2F */
    outb(PIC1_DATA, PIC_IRQ_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC_IRQ_OFFSET + 8);
    io_wait();

    /* ICW3: tell master PIC there is a slave at IRQ2, tell slave its ID */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086/88 mode */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Mask EVERY IRQ except IRQ0 (the PIT timer).
     * NOTE: we deliberately do NOT restore the firmware's old masks here.
     * QEMU's BIOS leaves several IRQ lines (e.g. IRQ1 keyboard) unmasked;
     * restoring those masks would let an IRQ with no IDT gate fire and
     * triple-fault the CPU the moment the device interrupts. Stage 2 only
     * needs the timer; the keyboard is polled. Later stages unmask more
     * IRQs explicitly, one driver at a time.                        */
    outb(PIC1_DATA, 0xFE);   /* 1111_1110b: only IRQ0 unmasked */
    outb(PIC2_DATA, 0xFF);   /* all slave IRQs masked          */
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}
