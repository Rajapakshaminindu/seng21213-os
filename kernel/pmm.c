/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager (PMM) implementation
 * File   : kernel/pmm.c
 *
 * Lecture 11 §2-3. A frame bitmap: one bit per 4 KB physical page.
 *   bit == 1  ->  frame is USED / reserved
 *   bit == 0  ->  frame is FREE and may be handed out
 *
 * Initialisation walks the BIOS E820 map (stored by boot/boot.asm at
 * 0x8000/0x8004), marks every usable frame inside [PMM_LOW_LIMIT,
 * PMM_HIGH_LIMIT) as free, and leaves everything else (the first 1 MB,
 * ROM/ACPI holes, anything above the cap) permanently marked used.
 *
 * pmm_alloc_frame() is a first-fit scan (L11 §3): it returns the lowest
 * free frame, marks it used and yields its physical address.
 * ============================================================================*/
#include "pmm.h"

/* 64 MB / 4 KB = 16384 frames -> 512 words of bitmap (2 KB, lives in BSS). */
static uint32_t frame_bitmap[PMM_MAX_FRAMES / 32];

static uint32_t total_frames;   /* usable frames under management          */
static uint32_t used_frames;    /* of those, currently allocated           */

static e820_entry_t e820[E820_MAX_ENTRIES];
static int          e820_count;

/* Frame index of the first / last managed frame */
#define FIRST_FRAME (PMM_LOW_LIMIT  / PAGE_SIZE)
#define LAST_FRAME  (PMM_HIGH_LIMIT / PAGE_SIZE)

/* --- bitmap primitives ----------------------------------------------------*/
static int  bit_used(uint32_t f)  { return (frame_bitmap[f >> 5] >> (f & 31)) & 1u; }
static void bit_mark_used(uint32_t f)  { frame_bitmap[f >> 5] |=  (1u << (f & 31)); }
static void bit_mark_free(uint32_t f)  { frame_bitmap[f >> 5] &= ~(1u << (f & 31)); }

/* --- initialisation -------------------------------------------------------*/
void pmm_init(void) {
    /* 1. Copy the E820 map out of the bootloader's fixed buffer. */
    uint16_t n = *(volatile uint16_t *)E820_COUNT_ADDR;
    if (n > E820_MAX_ENTRIES) n = E820_MAX_ENTRIES;
    e820_count = (int)n;

    for (int i = 0; i < e820_count; i++) {
        const uint8_t  *raw = (const uint8_t *)E820_ENTRY_ADDR + i * E820_ENTRY_SIZE;
        const uint64_t *q   = (const uint64_t *)raw;
        const uint32_t *d   = (const uint32_t *)(raw + 16);
        e820[i].base      = q[0];
        e820[i].length    = q[1];
        e820[i].type      = d[0];
        e820[i].extended  = d[1];
    }

    /* 2. Assume everything is used, then free the usable ranges. */
    for (uint32_t i = 0; i < PMM_MAX_FRAMES / 32; i++) {
        frame_bitmap[i] = 0xFFFFFFFFu;
    }
    total_frames = 0;
    used_frames  = 0;

    for (int i = 0; i < e820_count; i++) {
        if (e820[i].type != E820_USABLE) continue;

        uint64_t begin = e820[i].base;
        uint64_t end   = e820[i].base + e820[i].length;
        if (begin < PMM_LOW_LIMIT)  begin = PMM_LOW_LIMIT;
        if (end   > PMM_HIGH_LIMIT) end   = PMM_HIGH_LIMIT;
        begin = PAGE_ALIGN_UP(begin);
        end   = PAGE_ALIGN_DOWN(end);

        for (uint64_t a = begin; a < end; a += PAGE_SIZE) {
            uint32_t f = (uint32_t)(a / PAGE_SIZE);
            if (bit_used(f)) {          /* not yet counted as manageable */
                bit_mark_free(f);
                total_frames++;
            }
        }
    }
}

/* --- allocation (first-fit, L11 §3) ---------------------------------------*/
uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = FIRST_FRAME; f < LAST_FRAME; f++) {
        if (!bit_used(f)) {
            bit_mark_used(f);
            used_frames++;
            return f * PAGE_SIZE;
        }
    }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t paddr) {
    uint32_t f = paddr / PAGE_SIZE;
    if (f < FIRST_FRAME || f >= LAST_FRAME) return;
    if (!bit_used(f)) return;             /* double free – ignore */
    bit_mark_free(f);
    used_frames--;
}

/* --- statistics -----------------------------------------------------------*/
uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_used_frames(void)  { return used_frames;  }
uint32_t pmm_free_frames(void)  { return total_frames - used_frames; }

int pmm_e820_count(void) { return e820_count; }
const e820_entry_t *pmm_e820_entry(int i) {
    if (i < 0 || i >= e820_count) return (const e820_entry_t *)0;
    return &e820[i];
}
