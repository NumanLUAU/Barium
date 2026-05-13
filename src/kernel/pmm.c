#include <barium/pmm.h>
#include <barium/lib.h>
#include <barium/console.h>

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t last_search_index = 0;
static uint64_t highest_phys_address = 0;
static spinlock_t pmm_lock;

static void bitmap_set(uint64_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint64_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static int bitmap_test(uint64_t page) {
    return (bitmap[page / 8] & (1 << (page % 8))) != 0;
}

void pmm_init(barium_boot_info_t *info) {
    uint64_t memory_end = 0;
    b_efi_mem_desc *mmap = (b_efi_mem_desc*)info->memory_map;
    uint64_t desc_count = info->memory_map_size / info->descriptor_size;

    for (uint64_t i = 0; i < desc_count; i++) {
        b_efi_mem_desc *desc = (b_efi_mem_desc*)((uint64_t)mmap + (i * info->descriptor_size));
        if (desc->type != 7) continue;
        uint64_t end = desc->physical_start + (desc->number_of_pages * 4096);
        if (end > memory_end) memory_end = end;
    }

    highest_phys_address = memory_end;
    total_pages = memory_end / 4096;
    uint64_t bitmap_size = total_pages / 8;
    
    bitmap = 0;
    for (uint64_t i = 0; i < desc_count; i++) {
        b_efi_mem_desc *desc = (b_efi_mem_desc*)((uint64_t)mmap + (i * info->descriptor_size));
        if (desc->type == 7 && desc->number_of_pages * 4096 >= bitmap_size) {
            if (desc->physical_start >= 0x1000000) {
                bitmap = (uint8_t*)desc->physical_start;
                break;
            }
        }
    }

    if (!bitmap) {
        console_print("[pmm] no ram\n");
        return;
    }

    b_memset(bitmap, 0xFF, bitmap_size);
    free_pages = 0;

    for (uint64_t i = 0; i < desc_count; i++) {
        b_efi_mem_desc *desc = (b_efi_mem_desc*)((uint64_t)mmap + (i * info->descriptor_size));
        if (desc->type == 7) {
            for (uint64_t j = 0; j < desc->number_of_pages; j++) {
                uint64_t page = (desc->physical_start / 4096) + j;
                bitmap_clear(page);
                free_pages++;
            }
        }
    }

    uint64_t bitmap_start_page = (uint64_t)bitmap / 4096;
    uint64_t bitmap_pages = (bitmap_size + 4095) / 4096;
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(bitmap_start_page + i);
        free_pages--;
    }

    for (uint64_t i = 0; i < (0x1000000 / 4096); i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
        }
    }
}

void *pmm_alloc(uint64_t count) {
    uint64_t flags = b_irq_save();
    spin_lock(&pmm_lock);
    uint64_t start_index = last_search_index;
    for (uint64_t i = start_index; i < total_pages - count; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                found = 0;
                i += j; 
                break;
            }
        }

        if (found) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            free_pages -= count;
            last_search_index = i + count;
            spin_unlock(&pmm_lock);
            b_irq_restore(flags);
            return (void*)(i * 4096);
        }
    }

    for (uint64_t i = 0; i < start_index; i++) {
        int found = 1;
        for (uint64_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) {
                found = 0;
                i += j;
                break;
            }
        }

        if (found) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(i + j);
            }
            free_pages -= count;
            last_search_index = i + count;
            spin_unlock(&pmm_lock);
            b_irq_restore(flags);
            return (void*)(i * 4096);
        }
    }
    spin_unlock(&pmm_lock);
    b_irq_restore(flags);
    return (void*)0;
}

void pmm_free(void *ptr, uint64_t count) {
    uint64_t flags = b_irq_save();
    spin_lock(&pmm_lock);
    uint64_t page = (uint64_t)ptr / 4096;
    for (uint64_t i = 0; i < count; i++) {
        bitmap_clear(page + i);
        free_pages++;
    }
    spin_unlock(&pmm_lock);
    b_irq_restore(flags);
}

uint64_t pmm_get_free_memory() { return free_pages * 4096; }
uint64_t pmm_get_used_memory() { return (total_pages - free_pages) * 4096; }
uint64_t pmm_get_total_memory() { return total_pages * 4096; }
uint64_t pmm_get_highest_address() { return highest_phys_address; }
