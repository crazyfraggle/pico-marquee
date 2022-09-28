#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#include "bsp/board.h"

#include "pixels.h"
#include "demos.h"
#include "c64.h"

#include "demos/snek.h"

static int demo = 0;

// Forward declare demo renderers.
bool render_fire();
bool render_text();
void init_demo();
void init_fire();

// Interface
void select_demo(int num)
{
    if (num > 2)
        num = 0;
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

    render_text();

    return true;
}

// Text test demo
bool render_text()
{
    uint8_t *buf = get_render_buffer();

    for (int chr = 0; chr < 16; chr++)
    {
        for (int i = 0; i < 8; i++)
        {
            char chr_line = c64_font2[chr * 8 + i];
            for (int b = 0; b < 8; b++)
            {
                int bit = chr_line >> (7 - b) & 1;

                PIXEL_RED(buf, b + chr * 8, i) = bit ? 0x04 : 0x04;
                PIXEL_GRN(buf, b + chr * 8, i) = bit ? 0x88 : 0x00;
                PIXEL_BLU(buf, b + chr * 8, i) = bit ? 0xff : 0xaa;
            }
        }
    }
}
