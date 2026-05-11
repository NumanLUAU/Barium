#ifndef BARIUM_LIB_H
#define BARIUM_LIB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    volatile int lock;
} spinlock_t;

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
uint64_t b_irq_save();
void b_irq_restore(uint64_t flags);

void b_outb(uint16_t port, uint8_t val);
uint8_t b_inb(uint16_t port);
void b_io_wait();
void b_wrmsr(uint32_t msr, uint64_t val);
uint64_t b_rdmsr(uint32_t msr);
void b_memset(void *s, uint8_t c, size_t n);
int b_strcmp(const char *s1, const char *s2);
void b_memcpy(void *dest, const void *src, size_t n);
void b_strcpy(char *dest, const char *src);
int b_memcmp(const void *s1, const void *s2, size_t n);
void b_sleep(uint32_t ms);
uint64_t b_get_cr2();

#endif
