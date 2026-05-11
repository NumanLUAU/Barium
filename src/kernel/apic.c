#include <barium/apic.h>
#include <barium/lib.h>
#include <barium/vmm.h>
#include <barium/console.h>

#define APIC_BASE 0xFEE00000
#define APIC_REG_ID 0x20
#define APIC_REG_EOI 0x0B0
#define APIC_REG_SPURIOUS 0x0F0
#define APIC_REG_LVT_TIMER 0x320
#define APIC_REG_TIMER_INIT 0x380
#define APIC_REG_TIMER_CURRENT 0x390
#define APIC_REG_TIMER_DIVIDE 0x3E0
#define APIC_REG_ICR_LOW 0x300
#define APIC_REG_ICR_HIGH 0x310

static uint32_t *apic;
static volatile uint32_t ticks_per_ms;
static uint64_t global_ticks = 0;

static void apic_write(uint32_t reg, uint32_t val) {
    apic[reg / 4] = val;
}

static uint32_t apic_read(uint32_t reg) {
    return apic[reg / 4];
}

void apic_init(barium_boot_info_t *info) {
    pml4_t *pml4 = vmm_get_kernel_pml4();
    vmm_map(pml4, APIC_BASE, APIC_BASE, PAGE_PRESENT | PAGE_WRITABLE | PAGE_PCD);
    apic = (uint32_t*)APIC_BASE;

    apic_write(APIC_REG_SPURIOUS, apic_read(APIC_REG_SPURIOUS) | 0x1FF);

    apic_write(APIC_REG_TIMER_DIVIDE, 0x03);
    apic_write(APIC_REG_TIMER_INIT, 0xFFFFFFFF);

    uint8_t low = 0x00;
    uint8_t high = 0x4E;
    b_outb(0x43, 0x30);
    b_outb(0x40, low);
    b_outb(0x40, high);

    uint16_t last_pit = 0xFFFF;
    while (1) {
        b_outb(0x43, 0x00);
        uint8_t l = b_inb(0x40);
        uint8_t h = b_inb(0x40);
        uint16_t pit = (h << 8) | l;
        if (pit > last_pit) break;
        last_pit = pit;
    }

    uint32_t current = apic_read(APIC_REG_TIMER_CURRENT);
    ticks_per_ms = (0xFFFFFFFF - current) / 10;

    apic_write(APIC_REG_TIMER_INIT, 0);
}

void apic_init_ap() {
    apic_write(APIC_REG_SPURIOUS, apic_read(APIC_REG_SPURIOUS) | 0x1FF);
    apic_write(APIC_REG_LVT_TIMER, 0x20 | 0x20000); 
    apic_write(APIC_REG_TIMER_DIVIDE, 0x03);
    apic_write(APIC_REG_TIMER_INIT, ticks_per_ms * 10);
}

void apic_eoi() {
    apic_write(APIC_REG_EOI, 0);
}

void apic_send_ipi(uint32_t lapic_id, uint32_t val) {
    apic_write(APIC_REG_ICR_HIGH, lapic_id << 24);
    apic_write(APIC_REG_ICR_LOW, val);
}

uint32_t apic_get_id() {
    return apic_read(APIC_REG_ID) >> 24;
}

void apic_delay_ms(uint32_t ms) {
    uint32_t target = ticks_per_ms * ms;
    apic_write(APIC_REG_TIMER_INIT, target);
    while (apic_read(APIC_REG_TIMER_CURRENT) > 0);
}

void apic_timer_handler() {
    global_ticks++;
    apic_write(APIC_REG_EOI, 0);
}

uint64_t apic_get_ticks() {
    return global_ticks;
}
