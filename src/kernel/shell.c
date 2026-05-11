#include <barium/console.h>
#include <barium/keyboard.h>
#include <barium/lib.h>
#include <barium/gdt.h>
#include <barium/pmm.h>
#include <barium/idt.h>
#include <barium/vmm.h>
#include <barium/heap.h>
#include <barium/apic.h>
#include <barium/sched.h>
#include <barium/syscall.h>
#include <barium/acpi.h>

extern uint64_t get_cs();
extern uint64_t get_ds();
extern uint64_t get_ss();
extern uint64_t get_tr();
extern uint64_t get_lar(uint64_t selector);
extern uint64_t get_lsl(uint64_t selector);

void user_mode_worker() {
    char *msg = "hello from ring 3\n";
    __asm__ volatile(
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "syscall\n"
        "mov $2, %%rax\n"
        "syscall\n"
        : : "r"(msg) : "rax", "rdi", "rcx", "r11"
    );
}

volatile int vmm_test_active = 0; //may be a bad idea in the future but for now its fine

extern uint64_t get_cs();
extern uint64_t get_ds();
extern uint64_t get_ss();

void gdttest() {
    console_print("testing gdt\n");
    uint64_t cs = get_cs();
    uint64_t ds = get_ds();
    console_print("cs=");
    console_print_hex(cs);
    console_print(" ds=");
    console_print_hex(ds);
    console_newline();

    console_print("flushing segments\n");
    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        : : : "rax", "memory"
    );
    __asm__ volatile("mfence" : : : "memory");

    void *actual_base = gdt_get_base();
    gdt_entry_t *entries = (gdt_entry_t*)actual_base;
    if (entries[1].access & 0x01) console_print("k-code ok\n");
    if (entries[2].access & 0x01) console_print("k-data ok\n");

    console_print("gdt works\n");
}

void tsstest() {
    console_print("testing tss\n");
    uint16_t tr = (uint16_t)get_tr();
    console_print("tr=");
    console_print_hex(tr);
    console_newline();

    uint64_t tss_lar = get_lar(0x28);
    uint64_t tss_lsl = get_lsl(0x28);
    console_print("lar=");
    console_print_hex(tss_lar);
    console_print(" lsl=");
    console_print_hex(tss_lsl);
    console_newline();

    if (tss_lsl == 103) console_print("limit ok\n");
    if ((tss_lar & 0x0F00) == 0x0B00) console_print("tss busy\n");

    console_print("tss works\n");
}

void stress_worker_priv() {
    __asm__ volatile ("hlt");
}

void stress_worker_mem() {
    uint64_t *ptr = (uint64_t*)0;
    *ptr = 0xDEADBEEF;
}

void stress_worker_syscall() {
    for (int i = 0; i < 10; i++) {
        __asm__ volatile (
            "mov $1, %%rax\n"
            "mov %0, %%rdi\n"
            "syscall\n"
            : : "r"(".") : "rax", "rcx", "r11", "rdi"
        );
    }
    __asm__ volatile ("mov $2, %rax; syscall");
}

void ringtest() {
    console_print("starting excruciating ring 3 test\n");
    
    console_print("stage 1: syscall worker... ");
    uint64_t tid = sched_spawn_user(user_mode_worker, 10, "scall_worker");
    while (sched_is_alive(tid)) sched_yield();
    
    console_print("stage 2: privilege violation... ");
    tid = sched_spawn_user(stress_worker_priv, 10, "priv_worker");
    while (sched_is_alive(tid)) sched_yield();
    
    console_print("stage 3: memory violation... ");
    tid = sched_spawn_user(stress_worker_mem, 10, "mem_worker");
    while (sched_is_alive(tid)) sched_yield();
    
    console_print("stage 4: syscall spam test... ");
    tid = sched_spawn_user(stress_worker_syscall, 10, "spam_worker");
    while (sched_is_alive(tid)) sched_yield();
    
    console_print("ring 3 test complete\n");
}

void pmmtest() {
    console_print("testing pmm\n");
    uint64_t total = pmm_get_total_memory();
    uint64_t free = pmm_get_free_memory();
    console_print("ram=");
    console_print_hex(total);
    console_print(" free=");
    console_print_hex(free);
    console_newline();

    console_print("stage 1: alloc\n");
    void *p1 = pmm_alloc(1);
    if (!p1) { console_print("alloc failed\n"); return; }
    pmm_free(p1, 1);
    console_print("ok\n");

    console_print("stage 2: stress\n");
    void *pages[256];
    for (int i = 0; i < 256; i++) {
        pages[i] = pmm_alloc(1);
        *(uint64_t*)pages[i] = 0xDEADC0DE ^ (uint64_t)pages[i];
    }
    int ok = 1;
    for (int i = 0; i < 256; i++) if (*(uint64_t*)pages[i] != (0xDEADC0DE ^ (uint64_t)pages[i])) ok = 0;
    if (ok) console_print("pass\n");
    for (int i = 0; i < 256; i++) pmm_free(pages[i], 1);

    console_print("pmm works\n");
}

void idttest() {
    console_print("testing idt\n");
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    if (rflags & (1 << 9)) console_print("ints ok\n");
    else console_print("ints bad\n");

    console_print("stage 1: software path\n");
    __asm__ volatile("int $3");
    console_print("stage 1: ok\n");

    console_print("idt works\n");
}

void vmmtest() {
    console_print("testing vmm\n");

    pml4_t *pml4 = vmm_get_kernel_pml4();
    console_print("pml4=");
    console_print_hex((uint64_t)pml4);
    void *phys_page = pmm_alloc(1);
    
    console_print("stage 1: alias\n");
    vmm_map(pml4, 0xDEADBEEF000, (uint64_t)phys_page, PAGE_PRESENT | PAGE_WRITABLE);
    uint64_t *ptr1 = (uint64_t*)0xDEADBEEF000;
    *ptr1 = 0x12345678;
    
    if (*ptr1 == 0x12345678) {
        console_print("alias ok\n");
    } else {
        console_print("alias broke\n");
    }

    console_print("stage 2: protection\n");
    console_print("mapping read-only\n");
    vmm_map(pml4, 0xCAFEBABE000, (uint64_t)phys_page, PAGE_PRESENT);
    console_print("trying illegal write\n");
    
    vmm_test_active = 1;
    uint64_t *bad_ptr = (uint64_t*)0xCAFEBABE000;
    *bad_ptr = 0x666;
    vmm_test_active = 0;

    if (*bad_ptr == 0x666) {
        console_print("stage 2: ok (recovered)\n");
    } else {
        console_print("stage 2: failed\n");
    }
}

void heaptest() {
    console_print("testing heap\n");

    console_print("stage 1: fragmentation\n");
    void *a = kmalloc(128);
    void *b = kmalloc(256);
    void *c = kmalloc(128);
    console_print("alloc a, b, c. free b\n");
    kfree(b);
    
    void *d = kmalloc(200);
    if (d == b) console_print("hole reclaim ok\n");
    else console_print("hole reclaim failed\n");
    
    kfree(a); kfree(c); kfree(d);

    console_print("stage 2: coalescing\n");
    void *p1 = kmalloc(128);
    void *p2 = kmalloc(128);
    console_print("freeing p1, p2\n");
    kfree(p1);
    kfree(p2);
    
    void *p3 = kmalloc(250);
    if (p3 == p1) console_print("coalesce ok\n");
    else console_print("coalesce failed\n");
    kfree(p3);

    console_print("stage 3: stress\n");
    void *ptrs[64];
    for(int i=0; i<64; i++) {
        ptrs[i] = kmalloc(32);
        *(uint32_t*)ptrs[i] = 0xAAAAAAAA ^ i;
    }
    int ok = 1;
    for(int i=0; i<64; i++) if(*(uint32_t*)ptrs[i] != (0xAAAAAAAA ^ i)) ok = 0;
    if(ok) console_print("works\n");
    for(int i=0; i<64; i++) kfree(ptrs[i]);

    console_print("heap works\n");
}

void timertest() {
    console_print("testing timer\n");
    console_print("ticks=");
    console_print_hex(apic_get_ticks());
    console_newline();

    console_print("stage 1: sleep\n");
    uint64_t start = apic_get_ticks();
    b_sleep(500);
    uint64_t end = apic_get_ticks();
    uint64_t diff = end - start;
    
    console_print("diff=");
    console_print_hex(diff);
    console_newline();
    
    if (diff >= 50 && diff <= 52) console_print("precision ok\n");
    else console_print("drift bad\n");

    console_print("stage 2: pulses\n");
    for(int i=0; i<5; i++) {
        console_print("pulse ");
        console_putc('1' + i);
        console_putc('\n');
        b_sleep(100);
    }
    
    console_print("timer works\n");
}

void integrity_thread() {
    uint64_t tid = sched_get_tid();
    uint64_t magic = 0xCAFEBABE ^ tid;
    uint64_t failed = 0;
    uint64_t badval = 0;

    __asm__ volatile (
        "mov %[magic], %%rax\n" "mov %[magic], %%rbx\n" "mov %[magic], %%rcx\n" "mov %[magic], %%rdx\n"
        "mov %[magic], %%rsi\n" "mov %[magic], %%rdi\n" "mov %[magic], %%r8\n" "mov %[magic], %%r9\n"
        "mov %[magic], %%r10\n" "mov %[magic], %%r11\n" "mov %[magic], %%r12\n"
        
        "mov $50000000, %%rcx\n"
        "1:\n" "dec %%rcx\n" "jnz 1b\n"
        
        "cmp %[magic], %%rax\n" "jne 2f\n"
        "cmp %[magic], %%rbx\n" "jne 3f\n"
        "cmp %[magic], %%r12\n" "jne 4f\n"
        "jmp 5f\n"
        "2: mov $1, %[failed]\n mov %%rax, %[badval]\n jmp 5f\n"
        "3: mov $2, %[failed]\n mov %%rbx, %[badval]\n jmp 5f\n"
        "4: mov $3, %[failed]\n mov %%r12, %[badval]\n jmp 5f\n"
        "5:\n"
        : [failed] "=r"(failed), [badval] "=r"(badval)
        : [magic] "r"(magic)
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "cc"
    );

    if (!failed) {
        console_print("tid ");
        console_print_hex(tid);
        console_print(" ok\n");
        sched_exit();
    } else {
        console_print("tid ");
        console_print_hex(tid);
        console_print(" broke! reg=");
        console_print_hex(failed);
        console_print(" val=");
        console_print_hex(badval);
        console_print(" exp=");
        console_print_hex(magic);
        console_newline();
        while(1);
    }
}

static uint64_t race_counts[3] = {0, 0, 0};
static int finish_order = 1;
void priority_racer() {
    uint64_t tid = sched_get_tid();
    int index = (int)(tid % 3);
    for (volatile int i = 0; i < 50000000; i++);
    
    __asm__ volatile ("cli");
    race_counts[index] = finish_order++;
    __asm__ volatile ("sti");
    
    sched_exit();
}

static volatile uint64_t rr_counts[3] = {0, 0, 0};
static volatile int rr_active = 1;

void rr_worker() {
    uint64_t tid = sched_get_tid();
    int index = (int)(tid % 3);
    while (rr_active) rr_counts[index]++;
    sched_exit();
}

void rr_timer() {
    b_sleep(1000);
    rr_active = 0;
    sched_exit();
}

static volatile uint64_t yield_counts[2] = {0, 0};
static volatile int yield_active = 1;

void greedy_worker() {
    while (yield_active) yield_counts[0]++;
    sched_exit();
}

void polite_worker() {
    while (yield_active) {
        yield_counts[1]++;
        sched_yield();
    }
    sched_exit();
}

void yield_timer() {
    b_sleep(1000);
    yield_active = 0;
    sched_exit();
}

void chaos_worker() {
    for (volatile int i = 0; i < 2000000; i++);
    sched_exit();
}

static volatile uint64_t age_counts[2] = {0, 0};
static volatile int age_active = 1;

void greedy_p31() {
    while (age_active) age_counts[0]++;
    sched_exit();
}

void patient_p0() {
    while (age_active) age_counts[1]++;
    sched_exit();
}

void age_timer() {
    b_sleep(6000);
    age_active = 0;
    sched_exit();
}

static volatile uint64_t block_counts[2] = {0, 0};
static volatile int block_active = 1;

void spinner_p10() {
    while (block_active) block_counts[0]++;
    sched_exit();
}

void sleeper_p10() {
    while (block_active) {
        block_counts[1]++;
        sched_sleep(500);
    }
    sched_exit();
}

void block_timer() {
    b_sleep(2000);
    block_active = 0;
    sched_exit();
}

void schedtest() {
    console_print("testing sched\n");
    
    console_print("stage 1: integrity\n");
    sched_spawn(integrity_thread, 10, "integrity_1");
    sched_spawn(integrity_thread, 10, "integrity_2");
    b_sleep(1000);

    console_print("\nstage 2: priority dominance\n");
    race_counts[0] = race_counts[1] = race_counts[2] = 0;
    finish_order = 1;
    
    sched_spawn(priority_racer, 31, "p31_racer");
    sched_spawn(priority_racer, 15, "p15_racer");
    sched_spawn(priority_racer, 0, "p0_racer");
    
    while (race_counts[0] == 0 || race_counts[1] == 0 || race_counts[2] == 0) {
        __asm__ volatile("hlt");
    }
    
    console_print("finish order: ");
    console_print_hex(race_counts[1]); 
    console_print(" / ");
    console_print_hex(race_counts[2]);
    console_print(" / ");
    console_print_hex(race_counts[0]);
    console_newline();

    console_print("\nstage 3: round-robin\n");
    rr_active = 1;
    rr_counts[0] = rr_counts[1] = rr_counts[2] = 0;
    sched_spawn(rr_worker, 10, "rr_1");
    sched_spawn(rr_worker, 10, "rr_2");
    sched_spawn(rr_worker, 10, "rr_3");
    sched_spawn(rr_timer, 10, "rr_timer");

    while (rr_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("executions:\n");
    console_print_hex(rr_counts[0]); console_print(" / ");
    console_print_hex(rr_counts[1]); console_print(" / ");
    console_print_hex(rr_counts[2]); console_newline();

    console_print("\nstage 4: voluntary preemption\n");
    yield_active = 1;
    yield_counts[0] = yield_counts[1] = 0;
    sched_spawn(greedy_worker, 10, "greedy");
    sched_spawn(polite_worker, 10, "polite");
    sched_spawn(yield_timer, 10, "yield_timer");

    while (yield_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(yield_counts[0]); console_print(" / ");
    console_print_hex(yield_counts[1]); console_newline();

    console_print("\nstage 5: memory leaks\n");
    for (int i = 0; i < 100; i++) sched_spawn(chaos_worker, 5, "chaos");
    
    sched_yield();
    
    console_print("stage 5 ok\n");

    console_print("\nstage 6: anti-starvation\n");
    age_active = 1;
    age_counts[0] = age_counts[1] = 0;
    sched_spawn(greedy_p31, 31, "greedy_p31");
    sched_spawn(patient_p0, 0, "patient_p0");
    sched_spawn(age_timer, 31, "age_timer");

    while (age_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(age_counts[0]); console_print(" / ");
    console_print_hex(age_counts[1]); console_newline();

    console_print("\nstage 7: blocking sleep\n");
    block_active = 1;
    block_counts[0] = block_counts[1] = 0;
    sched_spawn(spinner_p10, 10, "spinner");
    sched_spawn(sleeper_p10, 10, "sleeper");
    sched_spawn(block_timer, 10, "block_timer");

    while (block_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(block_counts[0]); console_print(" / ");
    console_print_hex(block_counts[1]); console_newline();

    console_print("\nstage 8\n");
    console_print("done\n");
    
    console_print("\nsched ok\n");
}

static void core_worker() {
    uint32_t id = apic_get_id();
    console_print("worker starting on core ");
    console_print_hex(id);
    console_newline();

    for (int i = 0; i < 5; i++) {
        b_sleep(500);
        id = apic_get_id();
        console_print("core ");
        console_print_hex(id);
        console_print(" tick\n");
    }
    
    console_print("worker done\n");
    sched_exit();
}

void coretest() {
    uint32_t count = acpi_get_cpu_count();
    uint32_t workers = count * 2;

    console_print("detected ");
    console_print_hex(count);
    console_print(" cores. spawning ");
    console_print_hex(workers);
    console_print(" workers\n");

    for (uint32_t i = 0; i < workers; i++) {
        sched_spawn(core_worker, 10, "worker");
    }
}

void shell_run() {
    char cmd[64];
    int pos = 0;
    console_print("shell active\n");
    while (1) {
        console_print("> ");
        pos = 0;
        b_memset(cmd, 0, 64);
        while (1) {
            char c = keyboard_get_char();
            if (c == 0) {
                sched_yield();
                continue;
            }
            if (c == '\n') { console_newline(); break; }
            else if (c == '\b') { if (pos > 0) { pos--; cmd[pos] = 0; console_backspace(); } }
            else if (c >= 32 && c <= 126) { if (pos < 63) { cmd[pos++] = (char)c; console_putc((char)c); } }
        }
        if (b_strcmp(cmd, "gdttest") == 0) gdttest();
        else if (b_strcmp(cmd, "tsstest") == 0) tsstest();
        else if (b_strcmp(cmd, "ringtest") == 0) ringtest();
        else if (b_strcmp(cmd, "pmmtest") == 0) pmmtest();
        else if (b_strcmp(cmd, "idttest") == 0) idttest();
        else if (b_strcmp(cmd, "vmmtest") == 0) vmmtest();
        else if (b_strcmp(cmd, "tasks") == 0) sched_print_tasks();
        else if (b_strcmp(cmd, "heaptest") == 0) heaptest();
        else if (b_strcmp(cmd, "timertest") == 0) timertest();
        else if (b_strcmp(cmd, "schedtest") == 0) schedtest();
        else if (b_strcmp(cmd, "coretest") == 0) coretest();
        else if (b_strcmp(cmd, "help") == 0) console_print("commands: help, tasks, coretest, gdttest, tsstest, ringtest, pmmtest, idttest, vmmtest, heaptest, timertest, schedtest\n");
        else if (pos > 0) console_print("unknown command\n");
    }
}
