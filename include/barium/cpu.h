#ifndef BARIUM_CPU_H
#define BARIUM_CPU_H

#include <stdint.h>

#include <barium/lib.h>
#include <barium/gdt.h>

struct thread;

typedef struct {
    uint64_t kernel_stack;
    uint64_t user_rsp;
    struct thread *sched_thread;
    struct thread *ready_queues[32];
    spinlock_t sched_lock;
    uint32_t cpu_id;
    gdt_entry_t gdt[16] __attribute__((aligned(16)));
    gdtr_t gdtr;
    tss_t tss __attribute__((aligned(16)));
} cpu_t;

void cpu_init();
cpu_t *cpu_get();
cpu_t *cpu_get_by_id(int id);
cpu_t *cpu_get_by_index(int index);
int cpu_get_count();

#endif
