/* =============================================================================
 * SENG21213-OS :: Kernel heap (kmalloc / kfree) implementation
 * File   : kernel/kheap.c
 *
 * Lecture 11 §5 (extension). First-fit variable-size allocator over a fixed
 * 64 KB virtual window (KHEAP_BASE) whose pages are wired to PMM frames at
 * init. Every block — free or allocated — carries a 12-byte header:
 *
 *      [ size | magic | next ]  followed by `size` bytes of payload
 *
 * Free blocks are kept in a singly-linked list ordered by address so that
 * kfree() can coalesce with both neighbours and avoid fragmentation.
 * kmalloc() splits a block when the leftover is big enough to hold another
 * header plus a useful payload.
 * ============================================================================*/
#include "kheap.h"
#include "pmm.h"
#include "vmm.h"

#define MAGIC_FREE  0xF4EEF4EEu
#define MAGIC_ALLOC 0xA110CA7Eu
#define HDR         sizeof(blk_t)
#define MIN_SPLIT   16u          /* smallest payload worth splitting for */

typedef struct blk {
    uint32_t     size;   /* payload capacity in bytes (excludes header)  */
    uint32_t     magic;
    struct blk  *next;   /* next free block (only meaningful when free)  */
} blk_t;

static blk_t   *free_head;
static uint32_t used_bytes;

static blk_t *payload_hdr(void *p) { return (blk_t *)((uint8_t *)p - HDR); }
static void  *hdr_payload(blk_t *b) { return (void *)((uint8_t *)b + HDR); }

/* Unlink `blk` from the free list (prev == 0 means it is the head). */
static void unlink_free(blk_t *prev, blk_t *blk) {
    if (prev) prev->next = blk->next;
    else      free_head  = blk->next;
    blk->next = (blk_t *)0;
}

void kheap_init(void) {
    /* Back the window with physical frames, one page at a time. */
    for (uint32_t i = 0; i < KHEAP_PAGES; i++) {
        uint32_t f = pmm_alloc_frame();
        vmm_map_page(KHEAP_BASE + i * 4096u, f, VMM_PRESENT | VMM_WRITE);
    }

    /* One big free block spanning the whole window. */
    free_head        = (blk_t *)KHEAP_BASE;
    free_head->size  = KHEAP_SIZE - HDR;
    free_head->magic = MAGIC_FREE;
    free_head->next  = (blk_t *)0;
    used_bytes       = 0;
}

void *kmalloc(uint32_t size) {
    if (size == 0) return (void *)0;
    size = (size + 3u) & ~3u;              /* round payload up to 4 bytes */

    blk_t *prev = (blk_t *)0;
    for (blk_t *b = free_head; b; prev = b, b = b->next) {
        if (b->magic != MAGIC_FREE || b->size < size) continue;

        if (b->size >= size + HDR + MIN_SPLIT) {
            /* Split: carve `size` off the front, leave the rest free. */
            blk_t *rest = (blk_t *)((uint8_t *)b + HDR + size);
            rest->size  = b->size - size - HDR;
            rest->magic = MAGIC_FREE;
            rest->next  = b->next;
            if (prev) prev->next = rest; else free_head = rest;

            b->size  = size;
            b->magic = MAGIC_ALLOC;
            b->next  = (blk_t *)0;
            used_bytes += size;
            return hdr_payload(b);
        }

        /* Use the whole block (leftover too small to split). */
        unlink_free(prev, b);
        b->magic = MAGIC_ALLOC;
        used_bytes += b->size;
        return hdr_payload(b);
    }
    return (void *)0;                      /* heap exhausted */
}

void kfree(void *ptr) {
    if (!ptr) return;
    blk_t *b = payload_hdr(ptr);
    if (b->magic != MAGIC_ALLOC) return;   /* not ours / double free */

    used_bytes -= b->size;
    b->magic = MAGIC_FREE;
    b->next  = (blk_t *)0;

    /* Insert in address order, coalescing with both neighbours. */
    blk_t *prev = (blk_t *)0;
    blk_t *cur  = free_head;
    while (cur && cur < b) { prev = cur; cur = cur->next; }

    /* Coalesce with the following block if physically adjacent. */
    if (cur && (uint8_t *)b + HDR + b->size == (uint8_t *)cur) {
        b->size += HDR + cur->size;
        b->next  = cur->next;
    } else {
        b->next  = cur;
    }

    /* Coalesce with the preceding block if physically adjacent. */
    if (prev && (uint8_t *)prev + HDR + prev->size == (uint8_t *)b) {
        prev->size += HDR + b->size;
        prev->next  = b->next;
    } else {
        if (prev) prev->next = b; else free_head = b;
    }
}

uint32_t kheap_used(void)     { return used_bytes; }
uint32_t kheap_capacity(void) { return KHEAP_SIZE; }
