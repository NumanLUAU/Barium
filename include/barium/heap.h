#ifndef BARIUM_HEAP_H
#define BARIUM_HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init();
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
