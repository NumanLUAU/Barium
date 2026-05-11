#include <barium/cpu.h>
#include <barium/lib.h>
#include <barium/heap.h>

void cpu_init() {
    cpu_t *cpu = (cpu_t*)kmalloc(sizeof(cpu_t));
    b_memset(cpu, 0, sizeof(cpu_t));
    b_wrmsr(0xC0000101, (uint64_t)cpu); 
    b_wrmsr(0xC0000102, 0); 
}

cpu_t *cpu_get() {
    return (cpu_t*)b_rdmsr(0xC0000101);
}
