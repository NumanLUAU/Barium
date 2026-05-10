#include <efi/efi.h>
#include <barium/boot.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

void *get_rsdp(efi_system_table *st) {
    efi_guid rsdp_guid = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
    for (uintn i = 0; i < st->number_of_table_entries; i++) {
        if (st->configuration_table[i].vendor_guid.data1 == rsdp_guid.data1 &&
            st->configuration_table[i].vendor_guid.data2 == rsdp_guid.data2 &&
            st->configuration_table[i].vendor_guid.data3 == rsdp_guid.data3) {
            return st->configuration_table[i].vendor_table;
        }
    }
    return NULL;
}

efi_status efi_main(efi_handle image_handle, efi_system_table *system_table) {
    system_table->con_out->clear_screen(system_table->con_out);
    system_table->con_out->output_string(system_table->con_out, L"barium booting (v0.4)\r\n");

    system_table->con_out->output_string(system_table->con_out, L"detecting graphics");
    efi_guid gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
    efi_graphics_output_protocol *gop;
    if (system_table->boot_services->locate_protocol(&gop_guid, NULL, (void**)&gop) != 0) {
        system_table->con_out->output_string(system_table->con_out, L"failed to load gop\r\n");
        while(1);
    }

    uintn max_mode = gop->mode->max_mode;
    uintn best_mode = 0;
    uint32_t max_pixels = 0;

    for (uintn i = 0; i < max_mode; i++) {
        efi_graphics_output_mode_information *info;
        uintn info_size;
        if (gop->query_mode(gop, i, &info_size, &info) == 0) {
            uint32_t pixels = info->horizontal_resolution * info->vertical_resolution;
            if (pixels > max_pixels) {
                max_pixels = pixels;
                best_mode = i;
            }
        }
    }
    gop->set_mode(gop, best_mode);
    system_table->con_out->output_string(system_table->con_out, L"gop loaded\r\n");

    barium_boot_info_t boot_info;
    boot_info.framebuffer_base = (uint32_t*)gop->mode->frame_buffer_base;
    boot_info.framebuffer_size = gop->mode->frame_buffer_size;
    boot_info.width = gop->mode->info->horizontal_resolution;
    boot_info.height = gop->mode->info->vertical_resolution;
    boot_info.pixels_per_scan_line = gop->mode->info->pixels_per_scan_line;

    system_table->con_out->output_string(system_table->con_out, L"finding acpi rsdp");
    boot_info.rsdp = get_rsdp(system_table);
    if (!boot_info.rsdp) {
        system_table->con_out->output_string(system_table->con_out, L"failed to find acpi rsdp\r\n");
        while(1);
    }
    system_table->con_out->output_string(system_table->con_out, L"acpi rsdp found\r\n");

    system_table->con_out->output_string(system_table->con_out, L"loading kernel");
    efi_file_protocol *root;
    efi_guid simple_fs_guid = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    efi_loaded_image_protocol *loaded_image;
    efi_guid loaded_image_guid = {0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    system_table->boot_services->handle_protocol(image_handle, &loaded_image_guid, (void**)&loaded_image);

    efi_simple_file_system_protocol *fs;
    system_table->boot_services->handle_protocol(loaded_image->device_handle, &simple_fs_guid, (void**)&fs);
    fs->open_volume(fs, &root);

    efi_file_protocol *kernel_file;
    if (root->open(root, &kernel_file, L"kernel.bin", 1, 0) != 0) {
        system_table->con_out->output_string(system_table->con_out, L"kernel.bin not found\r\n");
        while(1);
    }
    system_table->con_out->output_string(system_table->con_out, L"kernel.bin loaded\r\n");

    uint64_t kernel_addr = 0x100000;
    efi_status status = system_table->boot_services->allocate_pages(2, 2, 256, &kernel_addr);
    if (status != 0) {
        system_table->con_out->output_string(system_table->con_out, L"allocation failure\r\n");
        while(1);
    }
    
    efi_file_info *file_info;
    uintn info_size = sizeof(efi_file_info);
    system_table->boot_services->allocate_pool(2, info_size, (void**)&file_info);
    efi_guid file_info_guid = {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    kernel_file->get_info(kernel_file, &file_info_guid, &info_size, file_info);

    uintn kernel_size = file_info->file_size;
    kernel_file->read(kernel_file, &kernel_size, (void*)kernel_addr);

    system_table->con_out->output_string(system_table->con_out, L"exiting boot services");
    uintn map_size = 0;
    efi_memory_descriptor *map = NULL;
    uintn map_key;
    uintn desc_size;
    uint32_t desc_ver;

    system_table->boot_services->get_memory_map(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 2 * desc_size;
    system_table->boot_services->allocate_pool(2, map_size, (void**)&map);
    system_table->boot_services->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver);

    boot_info.memory_map = map;
    boot_info.memory_map_size = map_size;
    boot_info.descriptor_size = desc_size;

    system_table->boot_services->exit_boot_services(image_handle, map_key);

    void (*kernel_entry)(barium_boot_info_t*) = (void(*)(barium_boot_info_t*))kernel_addr;
    kernel_entry(&boot_info);

    while (1);
    return 0;
}
