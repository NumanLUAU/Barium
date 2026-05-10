#include <barium/cpu.h>
#include <barium/lib.h>
#include <barium/heap.h>

static cpu_t bsp_cpu;

void cpu_init() {
    b_memset(&bsp_cpu, 0, sizeof(cpu_t));
    
    b_wrmsr(0xC0000101, (uint64_t)&bsp_cpu);
    b_wrmsr(0xC0000102, (uint64_t)&bsp_cpu);
}

cpu_t *cpu_get() {
    return &bsp_cpu;
}
