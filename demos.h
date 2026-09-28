#ifndef DEMOS_H
#define DEMOS_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    DEMO_DOT = 0,
    DEMO_FIRE = 1,
    DEMO_SNEK = 2,
    DEMO_BRIGHT = 3,
    DEMO_TEXT = 4,
    DEMO_COUNT
};

bool render_demo();
void select_demo(int);
void set_bright_mode_level(uint8_t level);
bool demo_keyboard_handler(char c);

#endif /* DEMOS_H */