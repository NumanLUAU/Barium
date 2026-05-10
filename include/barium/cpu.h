#ifndef BARIUM_CPU_H
#define BARIUM_CPU_H

#include <stdint.h>

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_stack;
    uint64_t scratch;
} cpu_t;

void cpu_init();
cpu_t *cpu_get();

#endif
