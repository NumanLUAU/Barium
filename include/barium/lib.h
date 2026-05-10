#ifndef BARIUM_LIB_H
#define BARIUM_LIB_H

#include <stdint.h>
#include <stddef.h>

void b_outb(uint16_t port, uint8_t val);
uint8_t b_inb(uint16_t port);
void b_io_wait();
void b_wrmsr(uint32_t msr, uint64_t val);
uint64_t b_rdmsr(uint32_t msr);
void b_memset(void *s, uint8_t c, size_t n);
int b_strcmp(const char *s1, const char *s2);
void b_memcpy(void *dest, const void *src, size_t n);
void b_sleep(uint32_t ms);

#endif
