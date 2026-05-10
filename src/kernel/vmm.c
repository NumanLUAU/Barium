#include <barium/vmm.h>
#include <barium/pmm.h>
#include <barium/lib.h>
#include <barium/console.h>

static pml4_t *kernel_pml4;

void vmm_switch(pml4_t *pml4) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4));
}

void vmm_init(barium_boot_info_t *info) {
    kernel_pml4 = (pml4_t*)pmm_alloc();
    b_memset(kernel_pml4, 0, 4096);

    b_efi_mem_desc *mmap = (b_efi_mem_desc*)info->memory_map;
    uint64_t desc_count = info->memory_map_size / info->descriptor_size;

    for (uint64_t i = 0; i < desc_count; i++) {
        b_efi_mem_desc *desc = (b_efi_mem_desc*)((uint64_t)mmap + (i * info->descriptor_size));
        uint64_t flags = PAGE_PRESENT;
        
        if (desc->type == 7 || desc->type == 1 || desc->type == 2 || desc->type == 3 || desc->type == 4) {
            flags |= PAGE_WRITABLE | PAGE_USER;
        } else {
            flags |= PAGE_WRITABLE | PAGE_PCD | PAGE_PWT | PAGE_USER;
        }

        uint64_t start = desc->physical_start;
        uint64_t size = desc->number_of_pages * 4096;

        for (uint64_t j = 0; j < size; j += 4096) {
            vmm_map(kernel_pml4, start + j, start + j, flags);
        }
    }

    uint64_t fb_base = (uint64_t)info->framebuffer_base;
    uint64_t fb_size = info->framebuffer_size;
    for (uint64_t i = 0; i < fb_size; i += 4096) {
        vmm_map(kernel_pml4, fb_base + i, fb_base + i, PAGE_PRESENT | PAGE_WRITABLE | PAGE_PCD | PAGE_PWT | PAGE_USER);
    }

    vmm_switch(kernel_pml4);
}

void vmm_map(pml4_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        void *pdpt = pmm_alloc();
        b_memset(pdpt, 0, 4096);
        pml4[pml4_idx] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        void *pd = pmm_alloc();
        b_memset(pd, 0, 4096);
        pdpt[pdpt_idx] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    uint64_t *pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void *pt = pmm_alloc();
        b_memset(pt, 0, 4096);
        pd[pd_idx] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    uint64_t *pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);

    pt[pt_idx] = (phys & ~0xFFF) | flags;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_get_phys(pml4_t *pml4, uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    uint64_t *pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFF);
}

pml4_t *vmm_get_kernel_pml4() {
    return kernel_pml4;
}
