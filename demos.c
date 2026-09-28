#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#include "bsp/board.h"

#include "pixels.h"
#include "demos.h"
#include "text.h"

#include "demos/snek.h"

static int demo = 0;
static uint8_t bright_mode_level = 255;

// Forward declare demo renderers.
bool render_fire();
bool render_bright();
void init_demo();
void init_fire();

// Interface
void select_demo(int num)
{
    if (num >= DEMO_COUNT || num < 0)
        num = DEMO_DOT;
    demo = num;
    init_demo();
}

void init_demo()
{
    switch (demo)
    {
    case 1:
        init_fire();
        break;
    case 2:
        init_snek();
        break;
    case 3:
        clear_buffers();
        flip_buffer(false);
        break;
    case 4:
        // Keep whatever strings are already stored; render_textmode() wipes
        // the panel each frame anyway.
        break;
    default:
        break;
    }
}

bool render_demo()
{
    switch (demo)
    {
    case 1:
        return render_fire();
    case 2:
        return render_snek();
    case 3:
        return render_bright();
    case 4:
        return render_textmode();
    default:
        return false;
    }
}

bool demo_keyboard_handler(char c)
{
    switch (demo)
    {
    case 2:
        return snek_keyboard(c);
    default:
        return false;
    }
}

void set_bright_mode_level(uint8_t level)
{
    bright_mode_level = level;
}

bool render_bright()
{
    uint8_t *buf = get_render_buffer();

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            PIXEL_RED(buf, x, y) = bright_mode_level;
            PIXEL_GRN(buf, x, y) = bright_mode_level;
            PIXEL_BLU(buf, x, y) = bright_mode_level;
        }
    }

    return true;
}

// Demos

// Fire demo
// Utilizes the hidden 33rd row of pixels to seed the flame.
void init_fire()
{
    clear_buffers();
    flip_buffer(false);
}

bool render_fire()
{
    uint8_t *buf = get_render_buffer();

    uint32_t ms = board_millis();

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 1; x < WIDTH - 1; x++)
        {
            PIXEL_RED(buf, x, y) = (PIXEL_RED(buf, x, y) + PIXEL_RED(buf, x - 1, y + 1) + PIXEL_RED(buf, x, y + 1) + PIXEL_RED(buf, x + 1, y + 1)) >> 2;
            PIXEL_GRN(buf, x, y) = (PIXEL_GRN(buf, x - 1, y + 1) + PIXEL_GRN(buf, x, y + 1) + PIXEL_GRN(buf, x + 1, y + 1)) >> 2;
            PIXEL_BLU(buf, x, y) = (PIXEL_BLU(buf, x - 1, y + 1) + PIXEL_BLU(buf, x, y + 1) + PIXEL_BLU(buf, x + 1, y + 1)) >> 2;
        }
    }

    // Clear bottom row
    for (int x = 0; x < WIDTH; x++)
    {
        PIXEL_RED(buf, x, HEIGHT) = PIXEL_RED(buf, x, HEIGHT) >> 1;
        PIXEL_GRN(buf, x, HEIGHT) = PIXEL_GRN(buf, x, HEIGHT) >> 1;
        PIXEL_BLU(buf, x, HEIGHT) = PIXEL_BLU(buf, x, HEIGHT) >> 1;
    }

    // Add pixels to hidden row.
    int newPixs = rnd & 0x15;
    for (int i = 0; i < newPixs; i++)
    {
        int x = rnd & 127;
        PIXEL_RED(buf, x, HEIGHT) = 128 + (rnd & 0x7f);
        PIXEL_GRN(buf, x, HEIGHT) = rnd & 0x7f;
        PIXEL_BLU(buf, x, HEIGHT) = rnd & 0x7f;
    }

    return true;
}
