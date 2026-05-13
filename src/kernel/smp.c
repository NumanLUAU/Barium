#include <barium/smp.h>
#include <barium/acpi.h>
#include <barium/apic.h>
#include <barium/vmm.h>
#include <barium/heap.h>
#include <barium/lib.h>
#include <barium/console.h>
#include <barium/idt.h>
#include <barium/gdt.h>
#include <barium/sched.h>

extern uint8_t _binary_build_smp_boot_bin_start[];
extern uint8_t _binary_build_smp_boot_bin_end[];

void smp_ap_entry() {
    gdt_init_ap();
    cpu_init();
    idt_init_ap();
    apic_init_ap();
    sched_init();
    
    smp_data_t *data = (smp_data_t*)0x7000;
    data->status = 1;

    __asm__ volatile("sti");
    while (1) __asm__ volatile("hlt");
}

void smp_init() {
    size_t trampoline_size = (uintptr_t)_binary_build_smp_boot_bin_end - (uintptr_t)_binary_build_smp_boot_bin_start;
    b_memcpy((void*)0x8000, _binary_build_smp_boot_bin_start, trampoline_size);

    smp_data_t *data = (smp_data_t*)0x7000;
    data->pml4 = (uint64_t)vmm_get_kernel_pml4();
    data->entry_point = (uint64_t)smp_ap_entry;

    int cpu_count = acpi_get_cpu_count();
    uint32_t bsp_id = apic_get_id();

    for (int i = 0; i < cpu_count; i++) {
        uint32_t lapic_id = acpi_get_lapic_id(i);
        if (lapic_id == bsp_id) continue;

        console_print("waking core ");
        console_print_num(lapic_id);
        console_print("... ");

        data->stack_top = (uint64_t)kmalloc(8192) + 8192;
        data->status = 0;

        apic_send_ipi(lapic_id, 0x0000C500); 
        apic_delay_ms(10);
        
        apic_send_ipi(lapic_id, 0x00000600 | (0x8000 >> 12)); 
        apic_delay_ms(1);
        apic_send_ipi(lapic_id, 0x00000600 | (0x8000 >> 12)); 
        
        int timeout = 100;
        while (data->status == 0 && timeout-- > 0) apic_delay_ms(10);

        if (data->status == 1) {
            console_print("ok\n");
        } else {
            console_print("timeout\n");
        }
    }
}
