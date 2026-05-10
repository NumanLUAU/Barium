#include <barium/console.h>
#include <barium/lib.h>

static uint32_t *framebuffer;
static uint32_t width;
static uint32_t height;
static uint32_t stride;
static uint32_t cursor_x = 10;
static uint32_t cursor_y = 10;

#define FONT_SCALE 1
#define CHAR_WIDTH (8 * FONT_SCALE)
#define CHAR_HEIGHT (8 * FONT_SCALE)
#define CHAR_SPACING 1

void console_init(barium_boot_info_t *info) {
    framebuffer = (uint32_t*)info->framebuffer_base;
    width = info->width;
    height = info->height;
    stride = info->pixels_per_scan_line;
}

void console_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= width || y >= height) return;
    framebuffer[y * stride + x] = color;
}

void console_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            console_put_pixel(x + j, y + i, color);
        }
    }
}

void console_clear(uint32_t color) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            console_put_pixel(x, y, color);
        }
    }
    cursor_x = 10;
    cursor_y = 10;
}

extern uint8_t font8x8_basic[128][8];

void console_putchar(char c, uint32_t color) {
    if (c == '\n') {
        cursor_x = 10;
        cursor_y += CHAR_HEIGHT + 4;
    } else if (c == '\r') {
        cursor_x = 10;
    } else if (c == '\b') {
        if (cursor_x >= CHAR_WIDTH + CHAR_SPACING) {
            cursor_x -= CHAR_WIDTH + CHAR_SPACING;
            console_rect(cursor_x, cursor_y, CHAR_WIDTH, CHAR_HEIGHT, 0x1B1B1B);
        }
    } else {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (font8x8_basic[(uint8_t)c][i] & (1 << j)) {
                    if (FONT_SCALE > 1) {
                        console_rect(cursor_x + (j * FONT_SCALE), cursor_y + (i * FONT_SCALE), FONT_SCALE, FONT_SCALE, color);
                    } else {
                        console_put_pixel(cursor_x + j, cursor_y + i, color);
                    }
                }
            }
        }
        cursor_x += CHAR_WIDTH + CHAR_SPACING;
    }

    if (cursor_x + CHAR_WIDTH >= width) {
        cursor_x = 10;
        cursor_y += CHAR_HEIGHT + 4;
    }

    if (cursor_y + CHAR_HEIGHT >= height) {
        console_clear(0x1B1B1B);
    }
}

void console_putc(char c) { console_putchar(c, 0xFFFFFF); }
void console_newline() { console_putchar('\n', 0xFFFFFF); }
void console_backspace() { console_putchar('\b', 0xFFFFFF); }

void console_print(const char *s) {
    while (*s) console_putc(*s++);
}

void console_print_hex(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    console_print("0x");
    for (int i = 60; i >= 0; i -= 4) {
        console_putc(hex[(val >> i) & 0xF]);
    }
}
