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
#include <barium/acpi.h>
#include <barium/smp.h>

void kmain(barium_boot_info_t *info) {
    console_init(info);
    console_clear(0x1B1B1B);
    console_print("barium kernel v0.4\n");

    gdt_init();
    pmm_init(info);
    idt_init();
    vmm_init(info);
    acpi_init(info->rsdp);
    heap_init();
    apic_init(info);
    cpu_init();
    syscall_init();
    smp_init();
    apic_init_ap();
    sched_init();
    keyboard_init();

    sched_spawn(shell_run, 10, "shell");

    __asm__ volatile("sti");
    console_print("barium ready\n");

    while (1) __asm__ volatile("hlt");
}
