/* =============================================================================
 * SENG21213-OS :: VGA Text-Mode Driver
 * File   : kernel/vga.c
 * Purpose: Implements the VGA 80×25 colour text-mode output driver.
 *          Direct memory-mapped I/O – no BIOS calls in protected mode.
 * ============================================================================*/
#include "vga.h"
#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------------*/
static int     cursor_row  = 0;
static int     cursor_col  = 0;
static uint8_t cur_attr    = 0;   /* Current attribute byte */

/* I/O port helpers (inline assembly) */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ---------------------------------------------------------------------------
 * Hardware cursor update via VGA CRTC registers (ports 0x3D4 / 0x3D5)
 * --------------------------------------------------------------------------*/
static void update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Write a single cell to VGA memory */
static inline void vga_write_cell(int row, int col, char c, uint8_t attr) {
    volatile uint16_t *cell = VGA_ADDR + row * VGA_COLS + col;
    *cell = (uint16_t)((attr << 8) | (uint8_t)c);
}

/* ---------------------------------------------------------------------------
 * Scroll the screen up by one line when the cursor goes past row 24
 * --------------------------------------------------------------------------*/
static void scroll_up(void) {
    /* Move every row up by one */
    volatile uint16_t *vga = VGA_ADDR;
    for (int r = 0; r < VGA_ROWS - 1; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            vga[r * VGA_COLS + c] = vga[(r + 1) * VGA_COLS + c];
        }
    }
    /* Blank the last row */
    uint16_t blank = (uint16_t)((cur_attr << 8) | ' ');
    for (int c = 0; c < VGA_COLS; c++) {
        vga[(VGA_ROWS - 1) * VGA_COLS + c] = blank;
    }
    cursor_row = VGA_ROWS - 1;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/

void vga_init(void) {
    cur_attr   = VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK);
    cursor_row = 0;
    cursor_col = 0;
    vga_clear(VGA_BLACK);
}

void vga_clear(vga_color_t bg) {
    cur_attr = VGA_ATTR(VGA_LIGHT_GREY, bg);
    uint16_t blank = (uint16_t)((cur_attr << 8) | ' ');
    volatile uint16_t *vga = VGA_ADDR;
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++) {
        vga[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    cur_attr = VGA_ATTR(fg, bg);
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    } else if (c == '\b') {
        if (cursor_col > 0) cursor_col--;
        vga_write_cell(cursor_row, cursor_col, ' ', cur_attr);
    } else {
        vga_write_cell(cursor_row, cursor_col, c, cur_attr);
        cursor_col++;
        if (cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    }

    if (cursor_row >= VGA_ROWS) scroll_up();
    update_hw_cursor();
}

void vga_puts(const char *str) {
    if (!str) return;
    while (*str) vga_putchar(*str++);
}

void vga_puts_color(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved = cur_attr;
    vga_set_color(fg, bg);
    vga_puts(str);
    cur_attr = saved;
}

void vga_set_cursor(int row, int col) {
    cursor_row = (row < 0) ? 0 : (row >= VGA_ROWS ? VGA_ROWS - 1 : row);
    cursor_col = (col < 0) ? 0 : (col >= VGA_COLS ? VGA_COLS - 1 : col);
    update_hw_cursor();
}

/* Minimal vga_printf: supports %s, %c, %d, %u, %x */
static void print_uint(uint32_t n, int base) {
    char buf[32];
    int  i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n > 0) {
        int r = n % base;
        buf[i++] = (r < 10) ? ('0' + r) : ('a' + r - 10);
        n /= base;
    }
    while (i > 0) vga_putchar(buf[--i]);
}

void vga_printf(const char *fmt, ...) {
    /* Minimal va_args via GCC __builtin_va_list */
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') { vga_putchar(*fmt++); continue; }
        fmt++;
        switch (*fmt) {
            case 's': vga_puts(__builtin_va_arg(args, const char *)); break;
            case 'c': vga_putchar((char)__builtin_va_arg(args, int)); break;
            case 'd': {
                int v = __builtin_va_arg(args, int);
                if (v < 0) { vga_putchar('-'); v = -v; }
                print_uint((uint32_t)v, 10);
                break;
            }
            case 'u': print_uint(__builtin_va_arg(args, uint32_t), 10); break;
            case 'x': print_uint(__builtin_va_arg(args, uint32_t), 16); break;
            case '%': vga_putchar('%'); break;
            default:  vga_putchar(*fmt); break;
        }
        fmt++;
    }
    __builtin_va_end(args);
}

/* Draw a box outline using IBM box-drawing characters (CP437) */
void vga_draw_box(int row, int col, int height, int width, vga_color_t color) {
    uint8_t saved = cur_attr;
    vga_set_color(color, VGA_BLACK);

    /* Corners */
    vga_write_cell(row,          col,         0xC9, cur_attr); /* ╔ */
    vga_write_cell(row,          col+width-1, 0xBB, cur_attr); /* ╗ */
    vga_write_cell(row+height-1, col,         0xC8, cur_attr); /* ╚ */
    vga_write_cell(row+height-1, col+width-1, 0xBC, cur_attr); /* ╝ */

    /* Top / bottom edges */
    for (int c = col+1; c < col+width-1; c++) {
        vga_write_cell(row,          c, 0xCD, cur_attr); /* ═ */
        vga_write_cell(row+height-1, c, 0xCD, cur_attr);
    }
    /* Left / right edges */
    for (int r = row+1; r < row+height-1; r++) {
        vga_write_cell(r, col,         0xBA, cur_attr); /* ║ */
        vga_write_cell(r, col+width-1, 0xBA, cur_attr);
    }

    cur_attr = saved;
}
