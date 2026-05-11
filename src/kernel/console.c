#include <barium/console.h>
#include <barium/lib.h>

static uint32_t *framebuffer;
static uint32_t width;
static uint32_t height;
static uint32_t stride;
static uint32_t cursor_x = 10;
static uint32_t cursor_y = 10;
static spinlock_t console_lock;

#define FONT_SCALE 1
#define CHAR_WIDTH (8 * FONT_SCALE)
#define CHAR_HEIGHT (8 * FONT_SCALE)
#define CHAR_SPACING 1

extern uint8_t font8x8_basic[128][8];

void console_init(barium_boot_info_t *info) {
    framebuffer = (uint32_t*)info->framebuffer_base;
    width = info->width;
    height = info->height;
    stride = info->pixels_per_scan_line;
}

static void console_put_pixel_unlocked(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= width || y >= height) return;
    framebuffer[y * stride + x] = color;
}

static void console_rect_unlocked(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            console_put_pixel_unlocked(x + j, y + i, color);
        }
    }
}

static void console_putchar_unlocked(char c, uint32_t color) {
    if (c == '\n') {
        cursor_x = 10;
        cursor_y += CHAR_HEIGHT + 4;
    } else if (c == '\r') {
        cursor_x = 10;
    } else if (c == '\b') {
        if (cursor_x >= CHAR_WIDTH + CHAR_SPACING) {
            cursor_x -= CHAR_WIDTH + CHAR_SPACING;
            console_rect_unlocked(cursor_x, cursor_y, CHAR_WIDTH, CHAR_HEIGHT, 0x1B1B1B);
        }
    } else {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (font8x8_basic[(uint8_t)c][i] & (1 << j)) {
                    if (FONT_SCALE > 1) {
                        console_rect_unlocked(cursor_x + (j * FONT_SCALE), cursor_y + (i * FONT_SCALE), FONT_SCALE, FONT_SCALE, color);
                    } else {
                        console_put_pixel_unlocked(cursor_x + j, cursor_y + i, color);
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
        cursor_x = 10;
        cursor_y = 10;
    }
}

void console_clear(uint32_t color) {
    uint64_t flags = b_irq_save();
    spin_lock(&console_lock);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            console_put_pixel_unlocked(x, y, color);
        }
    }
    cursor_x = 10;
    cursor_y = 10;
    spin_unlock(&console_lock);
    b_irq_restore(flags);
}

void console_putc(char c) {
    uint64_t flags = b_irq_save();
    spin_lock(&console_lock);
    console_putchar_unlocked(c, 0xFFFFFF);
    spin_unlock(&console_lock);
    b_irq_restore(flags);
}

void console_newline() { console_putc('\n'); }
void console_backspace() { console_putc('\b'); }

void console_print(const char *s) {
    uint64_t flags = b_irq_save();
    spin_lock(&console_lock);
    console_print_unlocked(s);
    spin_unlock(&console_lock);
    b_irq_restore(flags);
}

void console_print_hex(uint64_t val) {
    uint64_t flags = b_irq_save();
    spin_lock(&console_lock);
    console_print_hex_unlocked(val);
    spin_unlock(&console_lock);
    b_irq_restore(flags);
}

void console_print_unlocked(const char *s) {
    while (*s) console_putchar_unlocked(*s++, 0xFFFFFF);
}

void console_print_hex_unlocked(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    console_putchar_unlocked('0', 0xFFFFFF);
    console_putchar_unlocked('x', 0xFFFFFF);
    for (int i = 60; i >= 0; i -= 4) {
        console_putchar_unlocked(hex[(val >> i) & 0xF], 0xFFFFFF);
    }
}

uint64_t console_lock_acquire() {
    uint64_t flags = b_irq_save();
    spin_lock(&console_lock);
    return flags;
}

void console_lock_release(uint64_t flags) {
    spin_unlock(&console_lock);
    b_irq_restore(flags);
}
