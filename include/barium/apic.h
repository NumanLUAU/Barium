#ifndef BARIUM_APIC_H
#define BARIUM_APIC_H

#include <stdint.h>
#include <barium/boot.h>

void apic_init(barium_boot_info_t *info);
void apic_timer_handler();
uint64_t apic_get_ticks();
void b_sleep(uint32_t ms);

#endif
