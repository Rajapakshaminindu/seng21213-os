/* =============================================================================
 * SENG21213-OS :: RAM Disk driver implementation  (VMM-backed)
 * File   : kernel/ramdisk.c
 * Purpose: Lecture 12 §2.
 *
 * Instead of a 1 MB static BSS array (which would be placed at physical
 * addresses 0x3F000-0x13F000 and overlap the VGA memory hole at 0xA0000
 * and BIOS ROM at 0xF0000, causing a triple fault during BSS zeroing), the
 * disk is backed by 256 PMM-allocated page frames mapped into the VMM
 * dynamic window at virtual address RD_VIRT_BASE (0xC0800000).
 *
 * rd_init() MUST be called after pmm_init() and vmm_init().
 * ============================================================================*/
#include "ramdisk.h"
#include "pmm.h"

static void rd_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

static void rd_memset(void *p, uint8_t v, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = v;
}

void rd_init(void) {
    /* Allocate 256 physical frames and map them contiguously at RD_VIRT_BASE.
     * PMM_LOW_LIMIT (1 MB) guarantees these frames are in normal extended RAM
     * (0x100000-0x4000000), completely clear of the VGA/ROM hole. */
    for (uint32_t i = 0; i < RD_TOTAL_BLOCKS; i++) {
        uint32_t f = pmm_alloc_frame();
        vmm_map_page(RD_VIRT_BASE + i * RD_BLOCK_SIZE, f, VMM_PRESENT | VMM_WRITE);
    }
    /* Zero all mapped blocks (frames may contain stale data). */
    rd_memset((void *)RD_VIRT_BASE, 0, RD_SIZE);
}

void rd_read_block(uint32_t blk, void *buf) {
    if (blk >= RD_TOTAL_BLOCKS) return;
    rd_memcpy(buf, (void *)(RD_VIRT_BASE + blk * RD_BLOCK_SIZE), RD_BLOCK_SIZE);
}

void rd_write_block(uint32_t blk, const void *buf) {
    if (blk >= RD_TOTAL_BLOCKS) return;
    rd_memcpy((void *)(RD_VIRT_BASE + blk * RD_BLOCK_SIZE), buf, RD_BLOCK_SIZE);
}

void rd_read_bytes(uint32_t off, void *buf, uint32_t len) {
    if (off >= RD_SIZE) return;
    if (off + len > RD_SIZE) len = RD_SIZE - off;
    rd_memcpy(buf, (void *)(RD_VIRT_BASE + off), len);
}

void rd_write_bytes(uint32_t off, const void *buf, uint32_t len) {
    if (off >= RD_SIZE) return;
    if (off + len > RD_SIZE) len = RD_SIZE - off;
    rd_memcpy((void *)(RD_VIRT_BASE + off), buf, len);
}
