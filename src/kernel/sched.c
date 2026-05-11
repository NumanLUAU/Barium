#include <barium/sched.h>
#include <barium/heap.h>
#include <barium/lib.h>
#include <barium/console.h>
#include <barium/gdt.h>
#include <barium/cpu.h>
#include <barium/shell.h>
#include <barium/apic.h>

static uint64_t next_tid = 1;
static spinlock_t global_sched_lock;
static uint32_t next_cpu_index = 0;

typedef struct {
    thread_t *sleep_queue;
} cpu_extra_t;

static cpu_extra_t cpu_extras[64];

#define current_thread (cpu_get()->sched_thread)

void sched_init() {
    cpu_t *cpu = cpu_get();
    
    uint64_t flags = b_irq_save();
    if (next_tid == 1) {
        global_sched_lock.lock = 0;
        b_memset(cpu_extras, 0, sizeof(cpu_extras));
    }
    
    thread_t *idle = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(idle, 0, sizeof(thread_t));
    void *stack = kmalloc(8192);
    
    spin_lock(&global_sched_lock);
    idle->tid = next_tid++;
    spin_unlock(&global_sched_lock);
    
    idle->state = THREAD_RUNNING;
    idle->priority = 0;
    b_strcpy(idle->name, "idle");
    idle->stack_limit = stack;
    idle->stack_top = (void*)((uint64_t)stack + 8192);
    idle->next = NULL;
    
    cpu->sched_thread = idle;
    b_memset(cpu->ready_queues, 0, sizeof(cpu->ready_queues));
    cpu->sched_lock.lock = 0;
    
    cpu_extras[cpu->cpu_id].sleep_queue = NULL;

    b_irq_restore(flags);
}

uint64_t sched_spawn(void (*entry)(), uint8_t priority, char *name) {
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(thread, 0, sizeof(thread_t));
    
    if (name) b_strcpy(thread->name, name);

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
    spin_lock(&global_sched_lock);
    thread->tid = next_tid++;
    
    uint32_t target_idx = next_cpu_index;
    next_cpu_index = (next_cpu_index + 1) % cpu_get_count();
    spin_unlock(&global_sched_lock);

    cpu_t *target_cpu = cpu_get_by_index(target_idx);
    
    thread->rsp = (uint64_t)ptr;
    thread->state = THREAD_READY;
    thread->priority = priority;
    thread->ticks_remaining = (MAX_PRIORITY - priority + 1) * 2;
    thread->stack_limit = stack;
    thread->stack_top = (void*)((uint64_t)stack + 8192);
    thread->core_affinity = (uint8_t)target_cpu->cpu_id;

    spin_lock(&target_cpu->sched_lock);
    thread->next = target_cpu->ready_queues[priority];
    target_cpu->ready_queues[priority] = thread;
    spin_unlock(&target_cpu->sched_lock);
    
    b_irq_restore(flags);
    return thread->tid;
}

uint64_t sched_spawn_user(void (*entry)(), uint8_t priority, char *name) {
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    b_memset(thread, 0, sizeof(thread_t));
    if (name) b_strcpy(thread->name, name);

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
    spin_lock(&global_sched_lock);
    thread->tid = next_tid++;
    uint32_t target_idx = next_cpu_index;
    next_cpu_index = (next_cpu_index + 1) % cpu_get_count();
    spin_unlock(&global_sched_lock);

    cpu_t *target_cpu = cpu_get_by_index(target_idx);

    thread->rsp = (uint64_t)ptr;
    thread->state = THREAD_READY;
    thread->priority = priority;
    thread->ticks_remaining = (MAX_PRIORITY - priority + 1) * 2;
    thread->stack_limit = kstack;
    thread->stack_top = (void*)((uint64_t)kstack + 8192);
    thread->user_stack_limit = ustack;
    thread->user_stack_top = (void*)((uint64_t)ustack + 8192);
    thread->core_affinity = (uint8_t)target_cpu->cpu_id;

    spin_lock(&target_cpu->sched_lock);
    thread->next = target_cpu->ready_queues[priority];
    target_cpu->ready_queues[priority] = thread;
    spin_unlock(&target_cpu->sched_lock);
    
    b_irq_restore(flags);
    return thread->tid;
}

static void sched_age_threads(cpu_t *cpu) {
    for (int p = 0; p < MAX_PRIORITY; p++) {
        thread_t *prev = NULL;
        thread_t *curr = cpu->ready_queues[p];
        while (curr) {
            if (curr->tid == 1) { prev = curr; curr = curr->next; continue; }
            curr->total_ticks++; 
            if (curr->total_ticks > 19) { 
                curr->total_ticks = 0;
                thread_t *aged = curr;
                if (prev) prev->next = curr->next;
                else cpu->ready_queues[p] = curr->next;
                curr = curr->next;
                if (aged->priority < MAX_PRIORITY) aged->priority++;
                aged->next = cpu->ready_queues[aged->priority];
                cpu->ready_queues[aged->priority] = aged;
                continue;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

uint64_t sched_reschedule(uint64_t current_rsp) {
    cpu_t *cpu = cpu_get();
    if (!cpu->sched_thread) return current_rsp;

    uint64_t flags = b_irq_save();
    spin_lock(&cpu->sched_lock);
    
    thread_t *current = cpu->sched_thread;
    current->rsp = current_rsp;
    
    uint64_t current_tick = apic_get_ticks();
    thread_t *prev_sleep = NULL;
    thread_t *curr_sleep = cpu_extras[cpu->cpu_id].sleep_queue;
    while (curr_sleep) {
        if (current_tick >= curr_sleep->wakeup_tick) {
            thread_t *awake = curr_sleep;
            if (prev_sleep) prev_sleep->next = curr_sleep->next;
            else cpu_extras[cpu->cpu_id].sleep_queue = curr_sleep->next;
            curr_sleep = curr_sleep->next;
            awake->state = THREAD_READY;
            awake->next = cpu->ready_queues[awake->priority];
            cpu->ready_queues[awake->priority] = awake;
        } else {
            prev_sleep = curr_sleep;
            curr_sleep = curr_sleep->next;
        }
    }

    if (current->ticks_remaining > 0) current->ticks_remaining--;
    sched_age_threads(cpu);

    for (int p = MAX_PRIORITY; p >= 0; p--) {
        if (cpu->ready_queues[p] != NULL) {
            if (p > current->priority || 
               (p == current->priority && current->ticks_remaining == 0) || 
                current->state != THREAD_RUNNING) {
                
                thread_t *next = cpu->ready_queues[p];
                cpu->ready_queues[p] = next->next;
                
                if (current->state == THREAD_RUNNING) {
                    current->state = THREAD_READY;
                    current->ticks_remaining = (MAX_PRIORITY - current->priority + 1) * 2;
                    current->next = NULL;
                    if (cpu->ready_queues[current->priority] == NULL) {
                        cpu->ready_queues[current->priority] = current;
                    } else {
                        thread_t *last = cpu->ready_queues[current->priority];
                        while (last->next) last = last->next;
                        last->next = current;
                    }
                } else if (current->state == THREAD_SLEEPING) {
                    current->next = cpu_extras[cpu->cpu_id].sleep_queue;
                    cpu_extras[cpu->cpu_id].sleep_queue = current;
                } else if (current->state == THREAD_DEAD && current->priority != 0) {
                    if (current->user_stack_limit) kfree(current->user_stack_limit);
                    kfree(current->stack_limit);
                    kfree(current);
                }

                next->state = THREAD_RUNNING;
                cpu->sched_thread = next;
                tss_set_stack((uint64_t)next->stack_top);
                cpu->kernel_stack = (uint64_t)next->stack_top;

                spin_unlock(&cpu->sched_lock);
                b_irq_restore(flags);
                return next->rsp;
            }
            break;
        }
    }

    spin_unlock(&cpu->sched_lock);
    b_irq_restore(flags);
    return current->rsp;
}

void sched_exit() {
    __asm__ volatile ("cli");
    cpu_t *cpu = cpu_get();
    if (cpu->sched_thread) {
        cpu->sched_thread->state = THREAD_DEAD;
    }
    __asm__ volatile ("sti");
    while(1) __asm__ volatile ("hlt"); 
}

void sched_yield() {
    __asm__ volatile ("int $32");
}

void sched_sleep(uint64_t ms) {
    __asm__ volatile ("cli");
    cpu_t *cpu = cpu_get();
    if (cpu->sched_thread) {
        cpu->sched_thread->state = THREAD_SLEEPING;
        cpu->sched_thread->wakeup_tick = apic_get_ticks() + (ms / 10);
    }
    __asm__ volatile ("sti");
    sched_yield();
}

uint64_t sched_get_tid() {
    cpu_t *cpu = cpu_get();
    return cpu->sched_thread ? cpu->sched_thread->tid : 0;
}

int sched_is_alive(uint64_t tid) {
    for (int i = 0; i < cpu_get_count(); i++) {
        cpu_t *cpu = cpu_get_by_index(i);
        uint64_t flags = b_irq_save();
        spin_lock(&cpu->sched_lock);
        
        if (cpu->sched_thread && cpu->sched_thread->tid == tid) {
            spin_unlock(&cpu->sched_lock);
            b_irq_restore(flags);
            return 1;
        }

        for (int p = 0; p <= MAX_PRIORITY; p++) {
            thread_t *curr = cpu->ready_queues[p];
            while (curr) {
                if (curr->tid == tid) {
                    spin_unlock(&cpu->sched_lock);
                    b_irq_restore(flags);
                    return 1;
                }
                curr = curr->next;
            }
        }
        
        thread_t *curr_sleep = cpu_extras[cpu->cpu_id].sleep_queue;
        while (curr_sleep) {
            if (curr_sleep->tid == tid) {
                spin_unlock(&cpu->sched_lock);
                b_irq_restore(flags);
                return 1;
            }
            curr_sleep = curr_sleep->next;
        }
        
        spin_unlock(&cpu->sched_lock);
        b_irq_restore(flags);
    }
    return 0;
}

void sched_print_tasks() {
    console_print("tid      pri  core state     name\n");
    console_print("----------------------------------\n");
    
    for (int i = 0; i < cpu_get_count(); i++) {
        cpu_t *cpu = cpu_get_by_index(i);
        uint64_t flags = b_irq_save();
        spin_lock(&cpu->sched_lock);

        if (cpu->sched_thread) {
            console_print_hex(cpu->sched_thread->tid);
            console_print("  ");
            console_print_hex(cpu->sched_thread->priority);
            console_print("  ");
            console_print_hex(cpu->cpu_id);
            console_print("  running   ");
            console_print(cpu->sched_thread->name);
            console_print("\n");
        }

        for (int p = 0; p <= MAX_PRIORITY; p++) {
            thread_t *curr = cpu->ready_queues[p];
            while (curr) {
                console_print_hex(curr->tid);
                console_print("  ");
                console_print_hex(curr->priority);
                console_print("  ");
                console_print_hex(cpu->cpu_id);
                console_print("  ready     ");
                console_print(curr->name);
                console_print("\n");
                curr = curr->next;
            }
        }

        thread_t *curr_sleep = cpu_extras[cpu->cpu_id].sleep_queue;
        while (curr_sleep) {
            console_print_hex(curr_sleep->tid);
            console_print("  ");
            console_print_hex(curr_sleep->priority);
            console_print("  ");
            console_print_hex(cpu->cpu_id);
            console_print("  sleeping  ");
            console_print(curr_sleep->name);
            console_print("\n");
            curr_sleep = curr_sleep->next;
        }
        
        spin_unlock(&cpu->sched_lock);
        b_irq_restore(flags);
    }
}
