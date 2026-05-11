#ifndef BARIUM_SMP_H
#define BARIUM_SMP_H

#include <stdint.h>

typedef struct {
    uint64_t pml4;
    uint64_t stack_top;
    uint64_t entry_point;
    volatile uint64_t status;
} __attribute__((packed)) smp_data_t;

void smp_init();

#endif
