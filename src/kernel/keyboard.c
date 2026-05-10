#include <barium/keyboard.h>
#include <barium/lib.h>

static char kbd_buffer[128];
static uint32_t kbd_head = 0;
static uint32_t kbd_tail = 0;

static const char scancode_table[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void keyboard_init() {
    kbd_head = 0;
    kbd_tail = 0;

    for (int i = 0; i < 1000 && (b_inb(0x64) & 1); i++) {
        b_inb(0x60);
        b_io_wait();
    }

    b_outb(0x64, 0x20);
    b_io_wait();
    uint8_t status = b_inb(0x60);
    status |= 0x01;
    b_outb(0x64, 0x60);
    b_io_wait();
    b_outb(0x60, status);
}

void keyboard_handler() {
    uint8_t scancode = b_inb(0x60);
    if (scancode < sizeof(scancode_table)) {
        char c = scancode_table[scancode];
        if (c != 0) {
            uint32_t next = (kbd_head + 1) % 128;
            if (next != kbd_tail) {
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }
        }
    }
}

char keyboard_get_char() {
    while (kbd_head == kbd_tail) {
        __asm__ volatile("pause");
    }
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % 128;
    return c;
}
