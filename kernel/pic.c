/* =============================================================================
 * SENG21213-OS :: 8259 PIC driver implementation
 * File   : kernel/pic.c
 * ============================================================================*/
#include "pic.h"
#include "../include/io.h"

void pic_remap(void) {
    /* Save current masks so we can restore them (we still mask everything
     * except IRQ0 for this stage; later stages may unmask more). */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

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

    /* Restore masks, then explicitly unmask only IRQ0 (the PIT timer) */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    /* Unmask IRQ0 (bit 0 of PIC1_DATA) — everything else stays masked for now. */
    outb(PIC1_DATA, (uint8_t)(inb(PIC1_DATA) & ~0x01));
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}
