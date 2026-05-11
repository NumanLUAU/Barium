#include <barium/heap.h>
#include <barium/pmm.h>
#include <barium/lib.h>

typedef struct heap_block {
    uint64_t size;
    struct heap_block *next;
    int free;
    uint32_t magic;
} heap_block_t;

#define HEAP_MAGIC 0xBA410101
static heap_block_t *heap_start = NULL;
static spinlock_t heap_lock;

void heap_init() {
    uint8_t *pool = (uint8_t*)pmm_alloc(1024);
    heap_start = (heap_block_t*)pool;
    heap_start->size = (1024 * 4096) - sizeof(heap_block_t);
    heap_start->next = NULL;
    heap_start->free = 1;
    heap_start->magic = HEAP_MAGIC;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    uint64_t flags = b_irq_save();
    spin_lock(&heap_lock);
    size = (size + 7) & ~7;
    
    heap_block_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            if (current->size > size + sizeof(heap_block_t) + 16) {
                heap_block_t *new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->next = current->next;
                new_block->free = 1;
                new_block->magic = HEAP_MAGIC;
                
                current->size = size;
                current->next = new_block;
            }
            current->free = 0;
            void *ret = (void*)((uint8_t*)current + sizeof(heap_block_t));
            spin_unlock(&heap_lock);
            b_irq_restore(flags);
            return ret;
        }
        current = current->next;
    }
    spin_unlock(&heap_lock);
    b_irq_restore(flags);
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    uint64_t flags = b_irq_save();
    spin_lock(&heap_lock);
    heap_block_t *block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    if (block->magic != HEAP_MAGIC) {
        spin_unlock(&heap_lock);
        b_irq_restore(flags);
        return;
    }
    
    block->free = 1;
    
    heap_block_t *current = heap_start;
    while (current) {
        if (current->free && current->next && current->next->free) {
            current->size += current->next->size + sizeof(heap_block_t);
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
    spin_unlock(&heap_lock);
    b_irq_restore(flags);
}
