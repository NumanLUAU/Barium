#ifndef BARIUM_EFI_H
#define BARIUM_EFI_H

#include <stdint.h>
#include <stdbool.h>

#define EFIAPI __attribute__((ms_abi))

typedef uint64_t uintn;
typedef uint64_t efi_status;
typedef void* efi_handle;

#define EFI_SUCCESS 0
#define EFI_ERR 0x8000000000000000
#define EFI_NOT_FOUND (EFI_ERR | 14)
#define EFI_BUFFER_TOO_SMALL (EFI_ERR | 5)
#define EFI_UNSUPPORTED (EFI_ERR | 3)

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} efi_guid;

typedef struct {
    efi_guid vendor_guid;
    void *vendor_table;
} efi_configuration_table;

struct _efi_simple_text_output_protocol;
typedef efi_status (EFIAPI *efi_text_reset)(struct _efi_simple_text_output_protocol *this, bool extended_verification);
typedef efi_status (EFIAPI *efi_text_output_string)(struct _efi_simple_text_output_protocol *this, uint16_t *string);
typedef efi_status (EFIAPI *efi_text_clear_screen)(struct _efi_simple_text_output_protocol *this);

typedef struct _efi_simple_text_output_protocol {
    efi_text_reset reset;
    efi_text_output_string output_string;
    void *test_string;
    void *query_mode;
    void *set_mode;
    void *attribute;
    efi_text_clear_screen clear_screen;
    void *set_cursor_position;
    void *enable_cursor;
    void *mode;
} efi_simple_text_output_protocol;

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} efi_graphics_pixel_format;

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    efi_graphics_pixel_format pixel_format;
    uint32_t pixel_information[4]; 
    uint32_t pixels_per_scan_line;
} efi_graphics_output_mode_information;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    efi_graphics_output_mode_information *info;
    uintn size_of_info;
    uint64_t frame_buffer_base;
    uintn frame_buffer_size;
} efi_graphics_output_protocol_mode;

struct _efi_graphics_output_protocol;
typedef efi_status (EFIAPI *efi_graphics_output_protocol_query_mode)(struct _efi_graphics_output_protocol *this, uint32_t mode_number, uintn *size_of_info, efi_graphics_output_mode_information **info);
typedef efi_status (EFIAPI *efi_graphics_output_protocol_set_mode)(struct _efi_graphics_output_protocol *this, uint32_t mode_number);

typedef struct _efi_graphics_output_protocol {
    efi_graphics_output_protocol_query_mode query_mode;
    efi_graphics_output_protocol_set_mode set_mode;
    void *blt;
    efi_graphics_output_protocol_mode *mode;
} efi_graphics_output_protocol;

typedef struct {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor;

typedef struct {
    char signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header;

struct _efi_boot_services;
typedef efi_status (EFIAPI *efi_allocate_pages)(uint32_t type, uint32_t memory_type, uintn pages, uint64_t *memory);
typedef efi_status (EFIAPI *efi_free_pages)(uint64_t memory, uintn pages);
typedef efi_status (EFIAPI *efi_get_memory_map)(uintn *memory_map_size, efi_memory_descriptor *memory_map, uintn *map_key, uintn *descriptor_size, uint32_t *descriptor_version);
typedef efi_status (EFIAPI *efi_allocate_pool)(uint32_t pool_type, uintn size, void **buffer);
typedef efi_status (EFIAPI *efi_free_pool)(void *buffer);
typedef efi_status (EFIAPI *efi_exit_boot_services)(efi_handle image_handle, uintn map_key);
typedef efi_status (EFIAPI *efi_stall)(uintn microseconds);
typedef efi_status (EFIAPI *efi_handle_protocol)(efi_handle handle, efi_guid *protocol, void **interface);
typedef efi_status (EFIAPI *efi_locate_protocol)(efi_guid *protocol, void *registration, void **interface);

typedef struct _efi_boot_services {
    efi_table_header header;
    void *raise_tpl;
    void *restore_tpl;
    efi_allocate_pages allocate_pages;
    efi_free_pages free_pages;
    efi_get_memory_map get_memory_map;
    efi_allocate_pool allocate_pool;
    efi_free_pool free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    efi_handle_protocol handle_protocol;
    void *p_reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    efi_exit_boot_services exit_boot_services;
    void *get_next_monotonic_count;
    efi_stall stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocol_per_handle;
    void *locate_handle_buffer;
    efi_locate_protocol locate_protocol;
    void *locate_device_path_protocol;
    void *install_configuration_table_ex;
} efi_boot_services;

typedef struct _efi_system_table efi_system_table;

typedef struct {
    uint32_t revision;
    efi_handle parent_handle;
    efi_system_table *system_table;
    efi_handle device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    void *unload;
} efi_loaded_image_protocol;

typedef struct {
    uint64_t size;
    uint64_t file_size;
    uint64_t physical_size;
    uint16_t create_time[7];
    uint16_t last_access_time[7];
    uint16_t last_modification_time[7];
    uint64_t attribute;
    uint16_t file_name[256];
} efi_file_info;

struct _efi_file_protocol;
typedef struct _efi_file_protocol efi_file_protocol;

typedef efi_status (EFIAPI *efi_file_open)(efi_file_protocol *this, efi_file_protocol **new_handle, uint16_t *file_name, uint64_t open_mode, uint64_t attributes);
typedef efi_status (EFIAPI *efi_file_close)(efi_file_protocol *this);
typedef efi_status (EFIAPI *efi_file_read)(efi_file_protocol *this, uintn *buffer_size, void *buffer);
typedef efi_status (EFIAPI *efi_file_get_info)(efi_file_protocol *this, efi_guid *information_type, uintn *buffer_size, void *buffer);

struct _efi_file_protocol {
    uint64_t revision;
    efi_file_open open;
    efi_file_close close;
    void *delete;
    efi_file_read read;
    void *write;
    void *get_position;
    void *set_position;
    efi_file_get_info get_info;
    void *set_info;
    void *flush;
};

struct _efi_simple_file_system_protocol;
typedef struct _efi_simple_file_system_protocol efi_simple_file_system_protocol;

typedef efi_status (EFIAPI *efi_simple_file_system_open_volume)(efi_simple_file_system_protocol *this, efi_file_protocol **root);

struct _efi_simple_file_system_protocol {
    uint64_t revision;
    efi_simple_file_system_open_volume open_volume;
};

typedef struct _efi_system_table {
    efi_table_header header;
    uint16_t *firmware_vendor;
    uint32_t firmware_revision;
    efi_handle console_in_handle;
    void *con_in;
    efi_handle console_out_handle;
    efi_simple_text_output_protocol *con_out;
    efi_handle standard_error_handle;
    void *std_err;
    void *runtime_services;
    efi_boot_services *boot_services;
    uintn number_of_table_entries;
    efi_configuration_table *configuration_table;
} efi_system_table;

#endif
