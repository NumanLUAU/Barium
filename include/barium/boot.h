#ifndef BARIUM_BOOT_H
#define BARIUM_BOOT_H

#include <stdint.h>

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} __attribute__((packed)) b_efi_mem_desc;

typedef struct {
    uint32_t *framebuffer_base;
    uint32_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
    
    void *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;

    void *rsdp;
} barium_boot_info_t;

#endif
