#include <barium/gdt.h>
#include <barium/lib.h>

static gdt_entry_t gdt[3] __attribute__((aligned(16)));
static gdtr_t gdtr;

extern void gdt_load(gdtr_t *ptr);

void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= gran & 0xF0;
    gdt[index].access = access;
}



void gdt_init() {
    b_memset(gdt, 0, sizeof(gdt));

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xA0);
    
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    gdt_load(&gdtr);
}

void *gdt_get_base() {
    return (void*)gdt;
}


