#include <barium/cpu.h>
#include <barium/lib.h>
#include <barium/heap.h>

static cpu_t b_cpu;

void cpu_init() {
    b_memset(&b_cpu, 0, sizeof(cpu_t));
    b_wrmsr(0xC0000101, (uint64_t)&b_cpu); 
    b_wrmsr(0xC0000102, 0); 
}

cpu_t *cpu_get() {
    return &b_cpu;
}
