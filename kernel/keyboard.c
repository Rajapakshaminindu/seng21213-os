/* =============================================================================
 * SENG21213-OS :: PS/2 Keyboard Driver Implementation
 * File   : kernel/keyboard.c
 * ============================================================================*/
#include "keyboard.h"
#include "vga.h"
#include "../include/types.h"

/* I/O ports */
#define KB_DATA_PORT   0x60    /* Read scan code / write command */
#define KB_STATUS_PORT 0x64    /* Read status / write command */
#define KB_STATUS_OBF  0x01    /* Output Buffer Full bit */

/* Inline port I/O */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ---------------------------------------------------------------------------
 * Scancode Set 1 → ASCII translation table (unshifted)
 * Index = scancode. 0 = non-printable / not mapped.
 * --------------------------------------------------------------------------*/
static const char sc_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,  /* F1-F10 */
    0, 0,                  /* NumLock, ScrollLock */
    '7','8','9','-','4','5','6','+','1','2','3','0','.', /* numpad */
    0,0,0,                 /* filler */
    0,0                    /* F11,F12 */
};

static const char sc_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0, ' ', 0,
};

static bool shift_held = false;

void kb_init(void) {
    /* Flush any stale data in the keyboard buffer */
    while (inb(KB_STATUS_PORT) & KB_STATUS_OBF) {
        inb(KB_DATA_PORT);
    }
}

char kb_getchar(void) {
    uint8_t sc;
    while (true) {
        /* Wait until output buffer is full (key available) */
        while (!(inb(KB_STATUS_PORT) & KB_STATUS_OBF));
        sc = inb(KB_DATA_PORT);

        if (sc & 0x80) {
            /* Key release: bit 7 set, clear modifier state */
            uint8_t release = sc & 0x7F;
            if (release == 0x2A || release == 0x36) shift_held = false;
            continue;
        }

        /* Key press */
        if (sc == 0x2A || sc == 0x36) { shift_held = true; continue; }

        /* Caps lock / ctrl / alt – ignored in Stage 0 */

        char c = shift_held ? sc_ascii_shift[sc] : sc_ascii[sc];
        if (c) return c;
    }
}

int kb_readline(char *buf, int len) {
    int i = 0;
    while (i < len - 1) {
        char c = kb_getchar();
        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            break;
        }
        if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
            continue;
        }
        buf[i++] = c;
        vga_putchar(c);
    }
    buf[i] = '\0';
    return i;
}
