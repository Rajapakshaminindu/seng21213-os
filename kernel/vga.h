/* =============================================================================
 * SENG21213-OS :: VGA Text-Mode Driver Header
 * File   : kernel/vga.h
 * Purpose: Declarations for the VGA 80x25 colour text-mode driver.
 *
 * VGA text memory layout (Physical address 0xB8000):
 *   Each character cell = 2 bytes: [ ASCII (lo) | Attribute (hi) ]
 *   Attribute byte:  [ BG(3 bits) | FG(4 bits) | Blink(1 bit) ]
 * ============================================================================*/
#ifndef VGA_H
#define VGA_H

#include "../include/types.h"

/* Screen dimensions */
#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_ADDR   ((volatile uint16_t *)0xB8000)

/* VGA colour constants */
typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color_t;

/* Build an attribute byte from foreground + background colours */
#define VGA_ATTR(fg, bg)  ((uint8_t)(((bg) << 4) | (fg)))

/* Public API ----------------------------------------------------------------*/
void vga_init(void);
void vga_clear(vga_color_t bg);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_puts_color(const char *str, vga_color_t fg, vga_color_t bg);
void vga_set_cursor(int row, int col);
void vga_printf(const char *fmt, ...);

/* Student extension hook – implement in a later lecture */
void vga_draw_box(int row, int col, int height, int width, vga_color_t color);

#endif /* VGA_H */
