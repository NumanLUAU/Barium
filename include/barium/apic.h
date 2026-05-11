#ifndef BARIUM_APIC_H
#define BARIUM_APIC_H

#include <stdint.h>
#include <barium/boot.h>

void apic_init(barium_boot_info_t *info);
void apic_init_ap();
void apic_send_ipi(uint32_t lapic_id, uint32_t val);
uint32_t apic_get_id();
void apic_delay_ms(uint32_t ms);
void apic_timer_handler();
uint64_t apic_get_ticks();
void apic_eoi();
void b_sleep(uint32_t ms);

#endif
