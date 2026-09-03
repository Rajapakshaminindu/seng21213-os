/* =============================================================================
 * SENG21213-OS :: RAM Disk driver  (revised)
 * File   : kernel/ramdisk.h
 * Purpose: Lecture 12 §2. The 1 MB virtual disk is backed by PMM frames
 *          mapped into the VMM dynamic window at RD_VIRT_BASE.
 *          rd_init() MUST be called after both pmm_init() and vmm_init().
 * ============================================================================*/
#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"
#include "vmm.h"

#define RD_BLOCK_SIZE   4096u
#define RD_TOTAL_BLOCKS 256u          /* 256 × 4 KB = 1 MB                 */
#define RD_SIZE         (RD_TOTAL_BLOCKS * RD_BLOCK_SIZE)

/* Virtual base for the 1 MB disk window inside the VMM dynamic region.
 * VMM_DYN_BASE = 0xC0000000; kheap uses 0xC0400000; demand-paging demo
 * uses 0xC0000000; disk sits at 0xC0800000 (PD entries 770-771).       */
#define RD_VIRT_BASE    (VMM_DYN_BASE + 0x00800000u)  /* 0xC0800000 */

void    rd_init(void);          /* allocate & map PMM frames; call after vmm_init */
void    rd_read_block (uint32_t blk, void *buf);
void    rd_write_block(uint32_t blk, const void *buf);
void    rd_read_bytes (uint32_t byte_off, void *buf, uint32_t len);
void    rd_write_bytes(uint32_t byte_off, const void *buf, uint32_t len);

#endif /* RAMDISK_H */
