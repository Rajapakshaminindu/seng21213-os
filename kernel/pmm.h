/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager (PMM)
 * File   : kernel/pmm.h
 * Purpose: Lecture 11 §2-3 — a bitmap-based physical page-frame allocator.
 *          One bit per 4 KB frame. The usable RAM layout is discovered from
 *          the BIOS E820 memory map, which the bootloader (boot/boot.asm)
 *          queries with INT 0x15 / EAX=0xE820 and stores at a known physical
 *          address before switching to protected mode.
 * ============================================================================*/
#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

/* --- Page geometry (L11 §2) --- */
#define PAGE_SIZE          4096u
#define PAGE_ALIGN_DOWN(a) ((uint32_t)(a) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(a)   (((uint32_t)(a) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* --- Where the bootloader parked the E820 map (see boot/boot.asm) --- */
#define E820_COUNT_ADDR    0x8000u   /* uint16: number of entries          */
#define E820_ENTRY_ADDR    0x8004u   /* first 24-byte entry                */
#define E820_ENTRY_SIZE    24
#define E820_MAX_ENTRIES   32
#define E820_USABLE        1         /* type 1 = usable RAM                */

/* --- Physical window the PMM manages ---
 * Below 1 MB is reserved (BIOS data, VGA 0xB8000, kernel stack 0x90000, the
 * kernel image itself and the E820 buffer). We cap management at 64 MB so the
 * frame bitmap stays tiny and so every managed frame lies inside the VMM's
 * identity-mapped window (see kernel/vmm.h).                              */
#define PMM_LOW_LIMIT      0x00100000u   /* 1 MB  */
#define PMM_HIGH_LIMIT     0x04000000u   /* 64 MB */
#define PMM_MAX_FRAMES     (PMM_HIGH_LIMIT / PAGE_SIZE)

/* One E820 address-range descriptor as stored by the bootloader */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable, 2 = reserved, 3 = ACPI reclaim, ... */
    uint32_t extended;  /* ACPI 3.x extended attributes (unused here)      */
} e820_entry_t;

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);          /* first-fit; returns paddr or 0   */
void     pmm_free_frame(uint32_t paddr);

uint32_t pmm_total_frames(void);         /* usable frames under management  */
uint32_t pmm_used_frames(void);          /* currently allocated             */
uint32_t pmm_free_frames(void);          /* total - used                    */

int      pmm_e820_count(void);
const e820_entry_t *pmm_e820_entry(int i);

#endif /* PMM_H */
