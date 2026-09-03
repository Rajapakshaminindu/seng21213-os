/* =============================================================================
 * SENG21213-OS :: Kernel heap (kmalloc / kfree)
 * File   : kernel/kheap.h
 * Purpose: Lecture 11 §5 (extension). A small variable-size, first-fit heap
 *          with block splitting and address-ordered coalescing, layered on
 *          top of the PMM (backing frames) and the VMM (a 64 KB window of
 *          virtual pages in the dynamic region). This is the "never call
 *          malloc" replacement the kernel coding convention asks for.
 * ============================================================================*/
#ifndef KHEAP_H
#define KHEAP_H

#include "../include/types.h"

/* Heap virtual window: page-directory index 769 (0xC0400000), 16 pages. */
#define KHEAP_BASE   0xC0400000u
#define KHEAP_PAGES  16
#define KHEAP_SIZE   (KHEAP_PAGES * 4096u)

void     kheap_init(void);
void    *kmalloc(uint32_t size);
void     kfree(void *ptr);
uint32_t kheap_used(void);        /* bytes currently allocated (payload)   */
uint32_t kheap_capacity(void);    /* KHEAP_SIZE                            */

#endif /* KHEAP_H */
