#ifndef BARIUM_PMM_H
#define BARIUM_PMM_H

#include <stdint.h>
#include <barium/boot.h>

void pmm_init(barium_boot_info_t *info);
void *pmm_alloc(uint64_t count);
void pmm_free(void *ptr, uint64_t count);

uint64_t pmm_get_free_memory();
uint64_t pmm_get_used_memory();
uint64_t pmm_get_total_memory();
uint64_t pmm_get_highest_address();

#endif
