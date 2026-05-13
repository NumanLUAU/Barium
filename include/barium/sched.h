#ifndef BARIUM_SCHED_H
#define BARIUM_SCHED_H

#include <stdint.h>

#define MAX_PRIORITY 31
#define DEFAULT_PRIORITY 10

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_DEAD
} thread_state_t;

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rsi, rdi, r8;
    uint64_t r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) context_t;

typedef struct thread {
    uint64_t tid;
    uint64_t rsp;
    thread_state_t state;
    uint8_t priority;
    char name[16];
    uint8_t core_affinity;
    uint64_t ticks_remaining;
    uint64_t total_ticks;
    uint64_t wakeup_tick;
    void *stack_limit;
    void *stack_top;
    void *user_stack_limit;
    void *user_stack_top;
    struct thread *next;
} thread_t;

void sched_init();
uint64_t sched_spawn(void (*entry)(), uint8_t priority, char *name);
uint64_t sched_spawn_user(void (*entry)(), uint8_t priority, char *name);
uint64_t sched_reschedule(uint64_t current_rsp);
uint64_t sched_get_tid();
int sched_is_alive(uint64_t tid);
void sched_print_tasks();
void sched_exit();
void sched_yield();
void sched_sleep(uint64_t ms);

void sched_handoff(uint64_t tid);

#endif
