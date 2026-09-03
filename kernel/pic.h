/* =============================================================================
 * SENG21213-OS :: 8259 Programmable Interrupt Controller (PIC) driver
 * File   : kernel/pic.h / pic.c
 * Purpose: Remaps the two 8259 PICs so that hardware IRQs 0-15 map to
 *          interrupt vectors 0x20-0x2F, avoiding the CPU's reserved
 *          exception vectors 0-31.               (Lecture 9 §2)
 * ============================================================================*/
#ifndef PIC_H
#define PIC_H

#include "../include/types.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_EOI    0x20   /* End-Of-Interrupt command */

/* IRQ0 (timer) becomes interrupt vector 0x20 after this remap. */
#define PIC_IRQ_OFFSET 0x20

void pic_remap(void);
void pic_send_eoi(uint8_t irq);

#endif /* PIC_H */
