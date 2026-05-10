#include <barium/idt.h>
#include <barium/console.h>
#include <barium/lib.h>
#include <barium/keyboard.h>
#include <barium/apic.h>
#include <barium/sched.h>

static idt_entry_t idt[256] __attribute__((aligned(16)));
static idtr_t idtr;

extern void *isr_stub_table[];
extern void idt_load(idtr_t *ptr);

static const char *exception_messages[] = {
    "division by zero", "debug", "nmi", "breakpoint",
    "overflow", "out of bounds", "invalid opcode", "no coprocessor",
    "double fault", "coprocessor segment overrun", "bad tss", "segment not present",
    "stack fault", "general protection fault", "page fault", "unknown interrupt",
    "coprocessor fault", "alignment check", "machine check", "simd floating point",
    "virtualization", "reserved", "reserved", "reserved",
    "reserved", "reserved", "reserved", "reserved",
    "reserved", "reserved", "security exception", "reserved"
};

void idt_set_gate(uint8_t vector, void *isr, uint8_t attributes) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = attributes;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

static void pic_remap() {
    b_outb(0x20, 0x11);
    b_io_wait();
    b_outb(0xA0, 0x11);
    b_io_wait();
    b_outb(0x21, 0x20);
    b_io_wait();
    b_outb(0xA1, 0x28);
    b_io_wait();
    b_outb(0x21, 0x04);
    b_io_wait();
    b_outb(0xA1, 0x02);
    b_io_wait();
    b_outb(0x21, 0x01);
    b_io_wait();
    b_outb(0xA1, 0x01);
    b_io_wait();
    b_outb(0x21, 0xFD);
    b_outb(0xA1, 0xFF);
}

void idt_init() {
    b_memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 48; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x8E);
    }
    pic_remap();
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;
    idt_load(&idtr);
}

uint64_t interrupt_handler(uint64_t rsp) {
    context_t *ctx = (context_t*)rsp;
    uint8_t int_no = (uint8_t)ctx->int_no;

    if (int_no == 0x20) {
        apic_timer_handler();
        rsp = sched_reschedule(rsp);
    } else if (int_no == 0x21) {
        keyboard_handler();
    } else if (int_no == 3 || int_no == 4) {
        console_print("[idt] idt diag called\n");
    } else if (int_no < 32) {
        console_print("\n\n!!! kernel panic !!!\n");
        console_print("exception: ");
        console_print(exception_messages[int_no]);
        console_print("\nerror code: ");
        console_print_hex(ctx->err_code);
        console_print("\nrip: ");
        console_print_hex(ctx->rip);
        console_print("\nhalted\n");
        while(1) __asm__ volatile("hlt");
    }

    if (int_no >= 32 && int_no <= 47) {
        if (int_no >= 40) b_outb(0xA0, 0x20);
        b_outb(0x20, 0x20);
    }

    return rsp;
}
