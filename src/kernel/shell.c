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
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
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
    void *p1 = pmm_alloc();
    if (!p1) { console_print("alloc failed\n"); return; }
    pmm_free(p1);
    console_print("ok\n");

    console_print("stage 2: stress\n");
    void *pages[256];
    for (int i = 0; i < 256; i++) {
        pages[i] = pmm_alloc();
        *(uint64_t*)pages[i] = 0xDEADC0DE ^ (uint64_t)pages[i];
    }
    int ok = 1;
    for (int i = 0; i < 256; i++) if (*(uint64_t*)pages[i] != (0xDEADC0DE ^ (uint64_t)pages[i])) ok = 0;
    if (ok) console_print("pass\n");
    for (int i = 0; i < 256; i++) pmm_free(pages[i]);

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
    console_newline();

    console_print("stage 1: alias\n");
    void *phys_page = pmm_alloc();
    uint64_t virt_addr = 0xDEADBEEF000;
    
    vmm_map(pml4, virt_addr, (uint64_t)phys_page, PAGE_PRESENT | PAGE_WRITABLE);
    console_print("mapped ");
    console_print_hex((uint64_t)phys_page);
    console_print(" -> ");
    console_print_hex(virt_addr);
    console_newline();

    uint64_t *ptr = (uint64_t*)virt_addr;
    *ptr = 0x1234567887654321;
    
    if (*(uint64_t*)phys_page == 0x1234567887654321) {
        console_print("alias ok\n");
    } else {
        console_print("alias broke\n");
    }

    console_print("stage 2: protection\n");
    console_print("mapping read-only\n");
    vmm_map(pml4, 0xCAFEBABE000, (uint64_t)phys_page, PAGE_PRESENT);
    console_print("trying illegal write\n");
    
    uint64_t *bad_ptr = (uint64_t*)0xCAFEBABE000;
    *bad_ptr = 0x666;

    console_print("stage 2: failed\n");
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
    sched_spawn(integrity_thread, 10);
    sched_spawn(integrity_thread, 10);
    b_sleep(1000);

    console_print("\nstage 2: priority dominance\n");
    race_counts[0] = race_counts[1] = race_counts[2] = 0;
    finish_order = 1;
    
    sched_spawn(priority_racer, 31);
    sched_spawn(priority_racer, 15);
    sched_spawn(priority_racer, 0);
    
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
    sched_spawn(rr_worker, 10);
    sched_spawn(rr_worker, 10);
    sched_spawn(rr_worker, 10);
    sched_spawn(rr_timer, 10);

    while (rr_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("executions:\n");
    console_print_hex(rr_counts[0]); console_print(" / ");
    console_print_hex(rr_counts[1]); console_print(" / ");
    console_print_hex(rr_counts[2]); console_newline();

    console_print("\nstage 4: voluntary preemption\n");
    yield_active = 1;
    yield_counts[0] = yield_counts[1] = 0;
    sched_spawn(greedy_worker, 10);
    sched_spawn(polite_worker, 10);
    sched_spawn(yield_timer, 10);

    while (yield_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(yield_counts[0]); console_print(" / ");
    console_print_hex(yield_counts[1]); console_newline();

    console_print("\nstage 5: memory leaks\n");
    for (int i = 0; i < 100; i++) sched_spawn(chaos_worker, 5);
    
    sched_yield();
    
    console_print("stage 5 ok\n");

    console_print("\nstage 6: anti-starvation\n");
    age_active = 1;
    age_counts[0] = age_counts[1] = 0;
    sched_spawn(greedy_p31, 31);
    sched_spawn(patient_p0, 0);
    sched_spawn(age_timer, 31);

    while (age_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(age_counts[0]); console_print(" / ");
    console_print_hex(age_counts[1]); console_newline();

    console_print("\nstage 7: blocking sleep\n");
    block_active = 1;
    block_counts[0] = block_counts[1] = 0;
    sched_spawn(spinner_p10, 10);
    sched_spawn(sleeper_p10, 10);
    sched_spawn(block_timer, 10);

    while (block_active) __asm__ volatile("hlt");
    b_sleep(100);

    console_print("counts:\n");
    console_print_hex(block_counts[0]); console_print(" / ");
    console_print_hex(block_counts[1]); console_newline();

    console_print("\nstage 8\n");
    console_print("done\n");
    
    console_print("\nsched ok\n");
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
            if (c == '\n') { console_newline(); break; }
            else if (c == '\b') { if (pos > 0) { pos--; cmd[pos] = 0; console_backspace(); } }
            else if (c >= 32 && c <= 126) { if (pos < 63) { cmd[pos++] = (char)c; console_putc((char)c); } }
        }
        if (b_strcmp(cmd, "gdttest") == 0) gdttest();
        else if (b_strcmp(cmd, "pmmtest") == 0) pmmtest();
        else if (b_strcmp(cmd, "idttest") == 0) idttest();
        else if (b_strcmp(cmd, "vmmtest") == 0) vmmtest();
        else if (b_strcmp(cmd, "heaptest") == 0) heaptest();
        else if (b_strcmp(cmd, "timertest") == 0) timertest();
        else if (b_strcmp(cmd, "schedtest") == 0) schedtest();
        else if (b_strcmp(cmd, "help") == 0) console_print("commands: help, gdttest, pmmtest, idttest, vmmtest, heaptest, timertest, schedtest\n");
        else if (pos > 0) console_print("unknown command\n");
    }
}
