/* =============================================================================
 * SENG21213-OS :: Virtual Memory Manager (VMM) implementation
 * File   : kernel/vmm.c
 *
 * Lecture 11 §4-5 (extension). Classic x86 32-bit two-level paging:
 *
 *      CR3 -> Page Directory (1024 entries)
 *              -> Page Table (1024 entries)  -> 4 KB physical page
 *
 * We statically allocate:
 *   * one page directory,
 *   * 16 page tables identity-mapping 0x00000000..0x03FFFFFF (64 MB), which
 *     covers the kernel image, its BSS, the 0x90000 stack, VGA 0xB8000 and
 *     every frame the PMM can ever hand out (PMM_HIGH_LIMIT == 64 MB),
 *   * 4 page tables for the dynamic window 0xC0000000..0xC3FFFFFF, left
 *     entirely unmapped at boot so the demand-paging demo and the kernel
 *     heap can wire pages in on demand.
 *
 * Because the identity map covers everything the kernel already touches,
 * setting CR0.PG is behaviour-preserving: virtual == physical everywhere
 * except inside the dynamic window.
 * ============================================================================*/
#include "vmm.h"
#include "pmm.h"

#define PG 4096u

#define IDENT_TABLES 16          /* 16 * 4 MB = 64 MB identity map          */
#define DYN_TABLES    4          /* 4  * 4 MB = 16 MB dynamic window        */
#define DYN_PDI      (VMM_DYN_BASE >> 22)      /* = 768                     */

static uint32_t page_dir[1024]                 __attribute__((aligned(4096)));
static uint32_t ident_tables[IDENT_TABLES][1024] __attribute__((aligned(4096)));
static uint32_t dyn_tables[DYN_TABLES][1024]     __attribute__((aligned(4096)));

static inline void write_cr3(uint32_t paddr) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(paddr) : "memory");
}

void vmm_invlpg(uint32_t va) {
    __asm__ __volatile__("invlpg %0" : : "m"(*(char *)va) : "memory");
}

/* Return the page table backing a page-directory index, or 0 if the index
 * belongs to neither the identity nor the dynamic region. */
static uint32_t *table_for_pdi(uint32_t pdi) {
    if (pdi < IDENT_TABLES)            return ident_tables[pdi];
    if (pdi >= DYN_PDI && pdi < DYN_PDI + DYN_TABLES)
        return dyn_tables[pdi - DYN_PDI];
    return (uint32_t *)0;
}

void vmm_init(void) {
    /* Identity map: PDE t -> table t; PTE e -> physical (t*4MB + e*4KB). */
    for (uint32_t t = 0; t < IDENT_TABLES; t++) {
        page_dir[t] = ((uint32_t)&ident_tables[t]) | VMM_PRESENT | VMM_WRITE;
        for (uint32_t e = 0; e < 1024; e++) {
            ident_tables[t][e] = (t * 0x400000u + e * PG) | VMM_PRESENT | VMM_WRITE;
        }
    }

    /* Dynamic window: PDEs present but every PTE left 0 (not present). */
    for (uint32_t i = 0; i < DYN_TABLES; i++) {
        page_dir[DYN_PDI + i] = ((uint32_t)&dyn_tables[i]) | VMM_PRESENT | VMM_WRITE;
        for (uint32_t e = 0; e < 1024; e++) dyn_tables[i][e] = 0;
    }

    /* Load CR3 (physical address == virtual here, identity not yet active)
     * and set CR0.PG. CR0.PE is already set by the bootloader. */
    write_cr3((uint32_t)&page_dir);
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;                 /* CR0.PG */
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

void vmm_map_page(uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t pdi = va >> 22;
    uint32_t pti = (va >> 12) & 0x3FF;
    uint32_t *tbl = table_for_pdi(pdi);
    if (!tbl) return;
    tbl[pti] = (pa & ~0xFFFu) | flags;
    vmm_invlpg(va);
}

void vmm_unmap_page(uint32_t va) {
    uint32_t pdi = va >> 22;
    uint32_t pti = (va >> 12) & 0x3FF;
    uint32_t *tbl = table_for_pdi(pdi);
    if (!tbl) return;
    tbl[pti] = 0;
    vmm_invlpg(va);
}

uint32_t vmm_translate(uint32_t va) {
    uint32_t pdi = va >> 22;
    uint32_t pti = (va >> 12) & 0x3FF;
    uint32_t *tbl = table_for_pdi(pdi);
    if (!tbl) return (uint32_t)-1;
    uint32_t pte = tbl[pti];
    if (!(pte & VMM_PRESENT)) return (uint32_t)-1;
    return (pte & ~0xFFFu) | (va & 0xFFFu);
}
