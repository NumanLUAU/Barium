#include <barium/boot.h>
#include <barium/console.h>
#include <barium/gdt.h>
#include <barium/idt.h>
#include <barium/vmm.h>
#include <barium/heap.h>
#include <barium/apic.h>
#include <barium/cpu.h>
#include <barium/syscall.h>
#include <barium/keyboard.h>
#include <barium/shell.h>
#include <barium/pmm.h>
#include <barium/sched.h>

void kmain(barium_boot_info_t *info) {
    console_init(info);
    console_clear(0x1B1B1B);
    console_print("barium kernel v0.4\n");

    gdt_init();
    pmm_init(info);
    idt_init();
    vmm_init(info);
    heap_init();
    apic_init(info);
    cpu_init();
    syscall_init();
    sched_init();
    keyboard_init();

    console_print("barium ready\n");

    __asm__ volatile("sti");

    while (1) __asm__ volatile("hlt");
}
