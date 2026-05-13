#include <barium/cpu.h>
#include <barium/lib.h>
#include <barium/heap.h>
#include <barium/apic.h>

static cpu_t *cpus[64];
static int cpu_count_actual = 0;

void cpu_init() {
    cpu_t *cpu = (cpu_t*)kmalloc(sizeof(cpu_t));
    b_memset(cpu, 0, sizeof(cpu_t));
    cpu->cpu_id = apic_get_id();
    cpus[cpu_count_actual++] = cpu;
    b_wrmsr(0xC0000101, (uint64_t)cpu); 
    b_wrmsr(0xC0000102, (uint64_t)cpu); 
}

cpu_t *cpu_get() {
    return (cpu_t*)b_rdmsr(0xC0000101);
}

cpu_t *cpu_get_by_id(int id) {
    for (int i = 0; i < cpu_count_actual; i++) {
        if (cpus[i]->cpu_id == (uint32_t)id) return cpus[i];
    }
    return NULL;
}

cpu_t *cpu_get_by_index(int index) {
    if (index >= cpu_count_actual) return NULL;
    return cpus[index];
}

int cpu_get_count() {
    return cpu_count_actual;
}
