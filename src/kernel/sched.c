#include <barium/sched.h>
#include <barium/heap.h>
#include <barium/lib.h>
#include <barium/console.h>
#include <barium/gdt.h>

extern uint64_t apic_get_ticks();

static thread_t *ready_queues[MAX_PRIORITY + 1];
static thread_t *sleep_queue = NULL;
static thread_t *current_thread = NULL;
static uint64_t next_tid = 1;
static uint64_t total_sched_ticks = 0;

void sched_init() {
    b_memset(ready_queues, 0, sizeof(ready_queues));
    sleep_queue = NULL;
    current_thread = NULL;
    next_tid = 1;
    total_sched_ticks = 0;
    
    thread_t *idle = (thread_t*)kmalloc(sizeof(thread_t));
    void *stack = kmalloc(8192);
    idle->tid = next_tid++;
    idle->state = THREAD_RUNNING;
    idle->priority = 0;
    idle->stack_limit = stack;
    idle->stack_top = (void*)((uint64_t)stack + 8192);
    idle->next = NULL;
    current_thread = idle;
}

uint64_t sched_spawn(void (*entry)(), uint8_t priority) {
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    void *stack = kmalloc(8192);
    uint64_t *ptr = (uint64_t*)((uint64_t)stack + 8192);
    
    *(--ptr) = 0x10;             
    *(--ptr) = (uint64_t)stack + 8192; 
    *(--ptr) = 0x202;            
    *(--ptr) = 0x08;             
    *(--ptr) = (uint64_t)entry;  
    
    *(--ptr) = 0; 
    *(--ptr) = 0; 
    
    for (int i = 0; i < 15; i++) *(--ptr) = 0;

    __asm__ volatile ("cli");
    thread->tid = next_tid++;
    thread->rsp = (uint64_t)ptr;
    thread->state = THREAD_READY;
    thread->priority = priority;
    thread->ticks_remaining = (MAX_PRIORITY - priority + 1) * 2;
    thread->total_ticks = 0;
    thread->stack_limit = stack;
    thread->stack_top = (void*)((uint64_t)stack + 8192);

    thread->next = ready_queues[priority];
    ready_queues[priority] = thread;
    __asm__ volatile ("sti");

    return thread->tid;
}



static void sched_age_threads() {
    for (int p = 0; p < MAX_PRIORITY; p++) {
        thread_t *prev = NULL;
        thread_t *curr = ready_queues[p];
        while (curr) {
            curr->total_ticks++; 
            if (curr->total_ticks > 19) { 
                curr->total_ticks = 0;
                thread_t *aged = curr;
                if (prev) prev->next = curr->next;
                else ready_queues[p] = curr->next;
                curr = curr->next;
                if (aged->priority < MAX_PRIORITY) {
                    aged->priority++;
                }
                aged->next = ready_queues[aged->priority];
                ready_queues[aged->priority] = aged;
                continue;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

uint64_t sched_reschedule(uint64_t current_rsp) {
    if (!current_thread) return current_rsp;

    current_thread->rsp = current_rsp;
    total_sched_ticks++;
    
    uint64_t current_tick = apic_get_ticks();
    thread_t *prev_sleep = NULL;
    thread_t *curr_sleep = sleep_queue;
    while (curr_sleep) {
        if (current_tick >= curr_sleep->wakeup_tick) {
            thread_t *awake = curr_sleep;
            if (prev_sleep) prev_sleep->next = curr_sleep->next;
            else sleep_queue = curr_sleep->next;
            curr_sleep = curr_sleep->next;
            
            awake->state = THREAD_READY;
            awake->next = ready_queues[awake->priority];
            ready_queues[awake->priority] = awake;
        } else {
            prev_sleep = curr_sleep;
            curr_sleep = curr_sleep->next;
        }
    }

    if (current_thread->ticks_remaining > 0) {
        current_thread->ticks_remaining--;
    }

    sched_age_threads();

    for (int p = MAX_PRIORITY; p >= 0; p--) {
        if (ready_queues[p] != NULL) {
            if (p > current_thread->priority || 
               (p == current_thread->priority && current_thread->ticks_remaining == 0) || 
                current_thread->state != THREAD_RUNNING) {
                
                thread_t *next = ready_queues[p];
                ready_queues[p] = next->next;

                if (p > current_thread->priority) {
                    console_print("[sched] switching to tid ");
                    console_print_hex(next->tid);
                    console_newline();
                }
                
                if (current_thread->state == THREAD_RUNNING) {
                    current_thread->state = THREAD_READY;
                    current_thread->ticks_remaining = (MAX_PRIORITY - current_thread->priority + 1) * 2;
                    current_thread->next = NULL;
                    
                    if (ready_queues[current_thread->priority] == NULL) {
                        ready_queues[current_thread->priority] = current_thread;
                    } else {
                        thread_t *last = ready_queues[current_thread->priority];
                        while (last->next) last = last->next;
                        last->next = current_thread;
                    }
                } else if (current_thread->state == THREAD_SLEEPING) {
                    current_thread->next = sleep_queue;
                    sleep_queue = current_thread;
                } else if (current_thread->state == 0 && current_thread->priority != 0) {
                    kfree(current_thread->stack_limit);
                    kfree(current_thread);
                }

                next->state = THREAD_RUNNING;
                current_thread = next;
                
                return current_thread->rsp;
            }
            break;
        }
    }

    return current_thread->rsp;
}

void sched_exit() {
    __asm__ volatile ("cli");
    if (current_thread) {
        current_thread->state = 0;
    }
    __asm__ volatile ("sti");
    while(1) __asm__ volatile ("hlt"); 
}

void sched_yield() {
    __asm__ volatile ("int $32");
}

void sched_sleep(uint64_t ms) {
    __asm__ volatile ("cli");
    if (current_thread) {
        current_thread->state = THREAD_SLEEPING;
        current_thread->wakeup_tick = apic_get_ticks() + (ms / 10);
    }
    __asm__ volatile ("sti");
    sched_yield();
}

uint64_t sched_get_tid() {
    return current_thread ? current_thread->tid : 0;
}
