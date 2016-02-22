//============================================================================
/// @file       map.h
/// @brief      Kernel's physical memory map.
//
// Copyright 2016 Brett Vickers.
// Use of this source code is governed by a BSD-style license that can
// be found in the MonkOS LICENSE file.
//============================================================================

#include <core.h>
#include <libc/string.h>
#include <kernel/x86/cpu.h>
#include <kernel/interrupt/interrupt.h>
#include <kernel/mem/map.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/table.h>

/// Structure used to track the growth of the kernel's page table.
typedef struct kptable
{
    page_t *root;                ///< The top-level (PML4) page table
    page_t *next_page;           ///< The next page to use when allocating
    page_t *term_page;           ///< Just beyond the last available page.
} kptable_t;

static kptable_t kptable;

/// Return flags for large-page leaf entries in level 3 (PDPT) and level 2
/// (PDT) tables.
static uint64_t
get_pdflags(uint32_t memtype)
{
    switch (memtype)
    {
        case MEMTYPE_ACPI_NVS:
        case MEMTYPE_UNCACHED:
            return PF_PRESENT | PF_RW | PF_PS | PF_PWT | PF_PCD;

        case MEMTYPE_BAD:
        case MEMTYPE_UNMAPPED:
            return 0;

        default:
            return PF_PRESENT | PF_RW | PF_PS;
    }
}

/// Return flags for entries in level 1 (PT) tables.
static uint64_t
get_ptflags(uint32_t memtype)
{
    switch (memtype)
    {
        case MEMTYPE_ACPI_NVS:
        case MEMTYPE_UNCACHED:
            return PF_PRESENT | PF_RW | PF_PS | PF_PWT | PF_PCD;

        case MEMTYPE_BAD:
        case MEMTYPE_UNMAPPED:
            return 0;

        default:
            return PF_PRESENT | PF_RW;
    }
}

static inline uint64_t
alloc_page()
{
    if (kptable.next_page == kptable.term_page)
        fatal();
    return (uint64_t)kptable.next_page++ | PF_PRESENT | PF_RW;
}

static void
create_huge_page(uint64_t addr, uint32_t memtype)
{
    uint64_t pml4te = PML4E(addr);
    uint64_t pdpte  = PDPTE(addr);

    page_t *pml4t = kptable.root;
    if (pml4t->entry[pml4te] == 0)
        pml4t->entry[pml4te] = alloc_page();

    page_t *pdpt = PGPTR(pml4t->entry[pml4te]);
    pdpt->entry[pdpte] = addr | get_pdflags(memtype);
}

static void
create_large_page(uint64_t addr, uint32_t memtype)
{
    uint64_t pml4te = PML4E(addr);
    uint64_t pdpte  = PDPTE(addr);
    uint64_t pde    = PDE(addr);

    page_t *pml4t = kptable.root;
    if (pml4t->entry[pml4te] == 0)
        pml4t->entry[pml4te] = alloc_page();

    page_t *pdpt = PGPTR(pml4t->entry[pml4te]);
    if (pdpt->entry[pdpte] == 0)
        pdpt->entry[pdpte] = alloc_page();

    page_t *pdt = PGPTR(pdpt->entry[pdpte]);
    pdt->entry[pde] = addr | get_pdflags(memtype);
}

static void
create_small_page(uint64_t addr, uint32_t memtype)
{
    uint64_t pml4te = PML4E(addr);
    uint64_t pdpte  = PDPTE(addr);
    uint64_t pde    = PDE(addr);
    uint64_t pte    = PTE(addr);

    page_t *pml4t = kptable.root;
    if (pml4t->entry[pml4te] == 0)
        pml4t->entry[pml4te] = alloc_page();

    page_t *pdpt = PGPTR(pml4t->entry[pml4te]);
    if (pdpt->entry[pdpte] == 0)
        pdpt->entry[pdpte] = alloc_page();

    page_t *pdt = PGPTR(pdpt->entry[pdpte]);
    if (pdt->entry[pde] == 0)
        pdt->entry[pde] = alloc_page();

    page_t *pt = PGPTR(pdt->entry[pde]);
    pt->entry[pte] = addr | get_ptflags(memtype);
}

static void
map_region(const memtable_t *table, const memregion_t *region)
{
    // Don't map bad (or unmapped) memory.
    if (region->type == MEMTYPE_UNMAPPED || region->type == MEMTYPE_BAD)
        return;

    // Don't map reserved regions beyond the last usable physical address.
    if (region->type == MEMTYPE_RESERVED &&
        region->addr >= table->last_usable)
        return;

    uint64_t addr = region->addr;
    uint64_t term = region->addr + region->size;

    // Create a series of pages that cover the region. Try to use the largest
    // page sizes possible to keep the page table small.
    while (addr < term) {
        uint64_t remain = term - addr;

        // Create a huge page (1GiB) if possible.
        if ((addr & (PAGE_SIZE_HUGE - 1)) == 0 &&
            (remain >= PAGE_SIZE_HUGE)) {
            create_huge_page(addr, region->type);
            addr += PAGE_SIZE_HUGE;
        }

        // Create a large page (2MiB) if possible.
        else if ((addr & (PAGE_SIZE_LARGE - 1)) == 0 &&
                 (remain >= PAGE_SIZE_LARGE)) {
            create_large_page(addr, region->type);
            addr += PAGE_SIZE_LARGE;
        }

        // Create a small page (4KiB).
        else {
            create_small_page(addr, region->type);
            addr += PAGE_SIZE;
        }
    }
}

void
map_memory()
{
    // Zero all page table memory.
    memzero((void *)MEM_KERNEL_PAGETABLE, MEM_KERNEL_PAGETABLE_SIZE);

    kptable.root      = (page_t *)MEM_KERNEL_PAGETABLE;
    kptable.next_page = (page_t *)(MEM_KERNEL_PAGETABLE + PAGE_SIZE);
    kptable.term_page = (page_t *)MEM_KERNEL_PAGETABLE_END;

    // For each memory region in the BIOS-supplied memory table, create
    // appropriate page table entries.
    const memtable_t *table = memtable();
    for (uint64_t r = 0; r < table->count; r++)
        map_region(table, &table->region[r]);

    // Install the new page table.
    uint64_t addr = (uint64_t)kptable.root;
    set_pagetable(addr);
}
