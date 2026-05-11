#include <barium/sched.h>
#include <barium/heap.h>
#include <barium/lib.h>
#include <barium/console.h>
#include <barium/gdt.h>
#include <barium/cpu.h>
#include <barium/shell.h>

extern uint64_t apic_get_ticks();

static thread_t *ready_queues[MAX_PRIORITY + 1];
static thread_t *sleep_queue = NULL;
static uint64_t next_tid = 1;
static uint64_t total_sched_ticks = 0;
static spinlock_t sched_lock;

#define current_thread ((thread_t*)cpu_get()->sched_thread)
#define set_current_thread(t) (cpu_get()->sched_thread = (void*)(t))

void sched_init() {
    thread_t *idle = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(idle, 0, sizeof(thread_t));
    void *stack = kmalloc(8192);
    
    uint64_t flags = b_irq_save();
    spin_lock(&sched_lock);
    if (next_tid == 1) {
        b_memset(ready_queues, 0, sizeof(ready_queues));
        sleep_queue = NULL;
        total_sched_ticks = 0;
    }
    
    idle->tid = next_tid++;
    idle->state = THREAD_RUNNING;
    idle->priority = 0;
    
    b_strcpy(idle->name, "idle");
    idle->stack_limit = stack;
    idle->stack_top = (void*)((uint64_t)stack + 8192);
    idle->next = NULL;
    set_current_thread(idle);
    
    spin_unlock(&sched_lock);
    b_irq_restore(flags);
}

uint64_t sched_spawn(void (*entry)(), uint8_t priority, char *name) {
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(thread, 0, sizeof(thread_t));
    
    if (name) {
        b_strcpy(thread->name, name);
    }

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

    uint64_t flags = b_irq_save();
    spin_lock(&sched_lock);
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
    spin_unlock(&sched_lock);
    b_irq_restore(flags);

    return thread->tid;
}

uint64_t sched_spawn_user(void (*entry)(), uint8_t priority, char *name) {
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(thread, 0, sizeof(thread_t));
    
    if (name) {
        int i;
        for (i = 0; i < 15 && name[i]; i++) thread->name[i] = name[i];
        thread->name[i] = '\0';
    }

    void *kstack = kmalloc(8192);
    void *ustack = kmalloc(8192);
    uint64_t *ptr = (uint64_t*)((uint64_t)kstack + 8192);
    
    *(--ptr) = 0x1B;             
    *(--ptr) = (uint64_t)ustack + 8192; 
    *(--ptr) = 0x202;            
    *(--ptr) = 0x23;             
    *(--ptr) = (uint64_t)entry;  
    
    *(--ptr) = 0; 
    *(--ptr) = 0; 
    
    for (int i = 0; i < 15; i++) *(--ptr) = 0;

    uint64_t flags = b_irq_save();
    spin_lock(&sched_lock);
    thread->tid = next_tid++;
    thread->rsp = (uint64_t)ptr;
    thread->state = THREAD_READY;
    thread->priority = priority;
    thread->ticks_remaining = (MAX_PRIORITY - priority + 1) * 2;
    thread->total_ticks = 0;
    thread->stack_limit = kstack;
    thread->stack_top = (void*)((uint64_t)kstack + 8192);
    thread->user_stack_limit = ustack;
    thread->user_stack_top = (void*)((uint64_t)ustack + 8192);

    thread->next = ready_queues[priority];
    ready_queues[priority] = thread;
    spin_unlock(&sched_lock);
    b_irq_restore(flags);

    return thread->tid;
}

static void sched_age_threads() {
    for (int p = 0; p < MAX_PRIORITY; p++) {
        thread_t *prev = NULL;
        thread_t *curr = ready_queues[p];
        while (curr) {
            if (curr->tid == 1) {
                prev = curr;
                curr = curr->next;
                continue;
            }
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

    uint64_t flags = b_irq_save();
    spin_lock(&sched_lock);
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
                } else if (current_thread->state == THREAD_DEAD && current_thread->priority != 0) {
                    if (current_thread->user_stack_limit) kfree(current_thread->user_stack_limit);
                    kfree(current_thread->stack_limit);
                    kfree(current_thread);
                }

                next->state = THREAD_RUNNING;
                set_current_thread(next);
                
                tss_set_stack((uint64_t)current_thread->stack_top);
                cpu_get()->kernel_stack = (uint64_t)current_thread->stack_top;

                spin_unlock(&sched_lock);
                b_irq_restore(flags);
                return current_thread->rsp;
            }
            break;
        }
    }

    spin_unlock(&sched_lock);
    b_irq_restore(flags);
    return current_thread->rsp;
}

void sched_exit() {
    __asm__ volatile ("cli");
    if (current_thread) {
        current_thread->state = THREAD_DEAD;
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
    while (sched_is_alive(current_thread->tid) && current_thread->state == THREAD_SLEEPING) { sched_yield(); }
}

uint64_t sched_get_tid() {
    return current_thread ? current_thread->tid : 0;
}

int sched_is_alive(uint64_t tid) {
    if (current_thread && current_thread->tid == tid) return 1;
    for (int p = 0; p <= MAX_PRIORITY; p++) {
        thread_t *curr = ready_queues[p];
        while (curr) {
            if (curr->tid == tid) return 1;
            curr = curr->next;
        }
    }
    thread_t *curr_sleep = sleep_queue;
    while (curr_sleep) {
        if (curr_sleep->tid == tid) return 1;
        curr_sleep = curr_sleep->next;
    }
    return 0;
}

void sched_print_tasks() {
    console_print("tid      pri  state     name\n");
    console_print("----------------------------\n");
    
    if (current_thread) {
        console_print_hex(current_thread->tid);
        console_print("  ");
        console_print_hex(current_thread->priority);
        console_print("  running   ");
        console_print(current_thread->name);
        console_print("\n");
    }

    for (int p = 0; p <= MAX_PRIORITY; p++) {
        thread_t *curr = ready_queues[p];
        while (curr) {
            if (curr != current_thread) {
                console_print_hex(curr->tid);
                console_print("  ");
                console_print_hex(curr->priority);
                console_print("  ready     ");
                console_print(curr->name);
                console_print("\n");
            }
            curr = curr->next;
        }
    }

    thread_t *curr_sleep = sleep_queue;
    while (curr_sleep) {
        console_print_hex(curr_sleep->tid);
        console_print("  ");
        console_print_hex(curr_sleep->priority);
        console_print("  sleeping  ");
        console_print(curr_sleep->name);
        console_print("\n");
        curr_sleep = curr_sleep->next;
    }
}
