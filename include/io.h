/* =============================================================================
 * SENG21213-OS :: Low-level x86 Port I/O helpers
 * File   : include/io.h
 * Purpose: Thin wrappers around the IN/OUT instructions, needed to program
 *          the 8259 PIC and the i8253 PIT for the Lecture 9 scheduler.
 * ============================================================================*/
#ifndef IO_H
#define IO_H

#include "types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Small delay used after PIC command bytes on some real hardware. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* IO_H */
