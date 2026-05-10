#ifndef BARIUM_VMM_H
#define BARIUM_VMM_H

#include <stdint.h>
#include <barium/boot.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_LARGE (1ULL << 7)

typedef uint64_t pml4_t;

void vmm_init(barium_boot_info_t *info);
void vmm_map(pml4_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_map_large(pml4_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t vmm_get_phys(pml4_t *pml4, uint64_t virt);
pml4_t* vmm_get_kernel_pml4();

#endif
