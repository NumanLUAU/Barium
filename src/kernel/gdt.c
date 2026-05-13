#include <barium/gdt.h>
#include <barium/lib.h>
#include <barium/cpu.h>

extern void gdt_load(gdtr_t *ptr);
extern void tss_load();

static void gdt_set_entry_at(gdt_entry_t *gdt, int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= gran & 0xF0;
    gdt[index].access = access;
}

static void gdt_set_tss_at(gdt_entry_t *gdt, int index, uint64_t base, uint32_t limit) {
    gdt_tss_entry_t *tss_entry = (gdt_tss_entry_t*)&gdt[index];
    gdt_set_entry_at(gdt, index, base & 0xFFFFFFFF, limit, 0x89, 0x40);
    tss_entry->base_highest = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;
}




void gdt_init() {
    cpu_t *cpu = cpu_get();
    b_memset(cpu->gdt, 0, sizeof(cpu->gdt));
    b_memset(&cpu->tss, 0, sizeof(cpu->tss));

    gdt_set_entry_at(cpu->gdt, 0, 0, 0, 0, 0);
    gdt_set_entry_at(cpu->gdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry_at(cpu->gdt, 2, 0, 0xFFFFFFFF, 0x92, 0xA0);
    gdt_set_entry_at(cpu->gdt, 3, 0, 0xFFFFFFFF, 0xF2, 0xA0);
    gdt_set_entry_at(cpu->gdt, 4, 0, 0xFFFFFFFF, 0xFA, 0xA0);
    
    gdt_set_tss_at(cpu->gdt, 5, (uint64_t)&cpu->tss, sizeof(tss_t) - 1);

    cpu->gdtr.limit = (7 * 8) - 1;
    cpu->gdtr.base = (uint64_t)cpu->gdt;

    gdt_load(&cpu->gdtr);
    tss_load();
}

void gdt_init_ap() {
    gdt_init();
}

void *gdt_get_base() {
    return (void*)cpu_get()->gdt;
}

void *tss_get_ptr() {
    return (void*)&cpu_get()->tss;
}

void tss_set_stack(uint64_t stack) {
    cpu_get()->tss.rsp0 = stack;
}


