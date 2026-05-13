#include <barium/acpi.h>
#include <barium/console.h>
#include <barium/lib.h>

static uint32_t cpu_apic_ids[64];
static int cpu_count = 0;

void acpi_init(void *rsdp_ptr) {
    acpi_rsdp_t *rsdp = (acpi_rsdp_t*)rsdp_ptr;
    acpi_header_t *xsdt = (acpi_header_t*)rsdp->xsdt_address;
    
    int entries = (xsdt->length - sizeof(acpi_header_t)) / 8;
    uint64_t *table_ptrs = (uint64_t*)((uint64_t)xsdt + sizeof(acpi_header_t));

    acpi_madt_t *madt = NULL;

    for (int i = 0; i < entries; i++) {
        acpi_header_t *header = (acpi_header_t*)table_ptrs[i];
        if (b_memcmp(header->signature, "APIC", 4) == 0) {
            madt = (acpi_madt_t*)header;
            break;
        }
    }

    if (!madt) return;

    uint8_t *ptr = (uint8_t*)madt + sizeof(acpi_madt_t);
    uint8_t *end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        acpi_madt_entry_t *entry = (acpi_madt_entry_t*)ptr;
        if (entry->type == 0) {
            acpi_madt_local_apic_t *lapic = (acpi_madt_local_apic_t*)ptr;
            if (lapic->flags & 1) {
                cpu_apic_ids[cpu_count++] = lapic->apic_id;
            }
        }
        ptr += entry->length;
    }

    console_print("acpi: ");
    console_print_num(cpu_count);
    console_print(" cores\n");
}

uint32_t acpi_get_lapic_id(int index) {
    return cpu_apic_ids[index];
}

int acpi_get_cpu_count() {
    return cpu_count;
}
