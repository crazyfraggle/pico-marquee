#ifndef DEMOS_H
#define DEMOS_H

#include <stdint.h>

bool render_demo();
void select_demo(int);
void set_bright_mode_level(uint8_t level);
bool demo_keyboard_handler(char c);

#endif /* DEMOS_H */