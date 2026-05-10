#ifndef BARIUM_KEYBOARD_H
#define BARIUM_KEYBOARD_H

#include <stdint.h>

void keyboard_init();
char keyboard_get_char();
void keyboard_handler();

#endif
