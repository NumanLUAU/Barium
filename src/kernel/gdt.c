#include <barium/gdt.h>
#include <barium/lib.h>

static gdt_entry_t gdt[7] __attribute__((aligned(16)));
static gdtr_t gdtr;
static tss_t tss __attribute__((aligned(16)));

extern void gdt_load(gdtr_t *ptr);
extern void tss_load();

void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= gran & 0xF0;
    gdt[index].access = access;
}

void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    gdt_tss_entry_t *tss_entry = (gdt_tss_entry_t*)&gdt[index];
    gdt_set_entry(index, base & 0xFFFFFFFF, limit, 0x89, 0x40);
    tss_entry->base_highest = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;
}

void gdt_init() {
    b_memset(gdt, 0, sizeof(gdt));
    b_memset(&tss, 0, sizeof(tss));

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xA0);
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xF2, 0xA0);
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xFA, 0xA0);
    
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    gdt_load(&gdtr);
    tss_load();
}

void *gdt_get_base() {
    return (void*)gdt;
}

void *tss_get_ptr() {
    return (void*)&tss;
}

void tss_set_stack(uint64_t stack) {
    tss.rsp0 = stack;
}
