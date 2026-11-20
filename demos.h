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
    DEMO_CLOCK = 5,
    DEMO_BLOCKS = 6,
    DEMO_SCROLLER = 7,
    DEMO_MORPH = 8,
    DEMO_COUNT
};

bool render_demo();
void select_demo(int);
int get_demo(void);

// Short lowercase names for the DEMO_* indices, used by the MQTT mode select.
// demo_from_name() returns -1 for an unknown name.
const char *demo_name(int num);
int demo_from_name(const char *name);
void set_bright_mode_level(uint8_t level);
bool demo_keyboard_handler(char c);

#endif /* DEMOS_H */