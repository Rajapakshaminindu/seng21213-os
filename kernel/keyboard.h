/* =============================================================================
 * SENG21213-OS :: PS/2 Keyboard Driver
 * File   : kernel/keyboard.h + keyboard.c
 * Stage 0: Polling-based keyboard input (no interrupts yet).
 *          In Lecture 9 you will replace this with an IRQ1 handler.
 * ============================================================================*/
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"

/* Maximum input line length */
#define KB_BUF_SIZE 256

void kb_init(void);

/* Read one character (blocks until a key is pressed) */
char kb_getchar(void);

/* Read a line into buf (up to len-1 chars), NUL-terminated.
 * Echoes characters to VGA. Returns number of chars read.
 * Student TODO (Lecture 9): convert to interrupt-driven. */
int  kb_readline(char *buf, int len);

#endif /* KEYBOARD_H */
