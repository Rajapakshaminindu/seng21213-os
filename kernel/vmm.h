/* =============================================================================
 * SENG21213-OS :: Virtual Memory Manager (VMM) — x86 two-level paging
 * File   : kernel/vmm.h
 * Purpose: Lecture 11 §4-5 (extension). Sets up a page directory + page tables
 *          and turns on CR0.PG. Two regions are provided:
 *            * an IDENTITY map of the first 64 MB (so all existing kernel
 *              code/data/stack/VGA keep working unchanged once paging is on),
 *            * a DYNAMIC window at VMM_DYN_BASE (0xC0000000) whose pages start
 *              unmapped and are wired up on demand with vmm_map_page() — this
 *              is what the demand-paging page-fault demo and the kernel heap
 *              use.
 * ============================================================================*/
#ifndef VMM_H
#define VMM_H

#include "../include/types.h"

/* Page-table entry / page-directory entry flags (Intel SDM Vol.3 Ch.4) */
#define VMM_PRESENT  0x001u
#define VMM_WRITE    0x002u
#define VMM_USER     0x004u

/* Base of the dynamic (initially-unmapped) virtual window. Page-directory
 * index 768..771 -> 0xC0000000..0xC3FFFFFF. */
#define VMM_DYN_BASE 0xC0000000u

void     vmm_init(void);                       /* build tables, set CR3, CR0.PG */
void     vmm_map_page(uint32_t va, uint32_t pa, uint32_t flags);
void     vmm_unmap_page(uint32_t va);
uint32_t vmm_translate(uint32_t va);           /* VA -> PA, or (uint32_t)-1     */
void     vmm_invlpg(uint32_t va);

#endif /* VMM_H */
