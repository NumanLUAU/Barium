#ifndef BARIUM_CONSOLE_H
#define BARIUM_CONSOLE_H

#include <stdint.h>
#include <barium/boot.h>

void console_init(barium_boot_info_t *info);
void console_clear(uint32_t color);
void console_putc(char c);
void console_backspace();
void console_print(const char *str);
void console_print_hex(uint64_t val);
void console_print_unlocked(const char *s);
void console_print_hex_unlocked(uint64_t val);
void console_newline();
uint64_t console_lock_acquire();
void console_lock_release(uint64_t flags);

#endif
