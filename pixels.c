#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "pixels.h"

// Statically allocate the shared buffer.
static uint8_t buf[2][(HEIGHT + 1) * WIDTH * 3];

static int render_buffer = 0;
// Create RBG 32 bit from R, G and B bytes.
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return r + (g * 256) + (b * 256 * 256);
}

void set_pixel(int x, int y, uint32_t bgr)
{
    PIXEL_RED(buf[render_buffer], x, y) = bgr & 0xff;
    PIXEL_GRN(buf[render_buffer], x, y) = (bgr >> 8) & 0xff;
    PIXEL_BLU(buf[render_buffer], x, y) = (bgr >> 16) & 0xff;
}

void flip_buffer(bool copy)
{
    render_buffer ^= 1;
    // Copy to second buffer. Required for manipulation of existing pixels, but can be skipped
    // if redrawing full frame.
    if (copy)
    {
        memcpy(buf[render_buffer], buf[render_buffer ^ 1], HEIGHT * WIDTH * 3);
    }
}

void clear_buffers()
{
    memset(buf[0], 0, WIDTH * HEIGHT * 3);
    memset(buf[1], 0, WIDTH * HEIGHT * 3);
}

uint8_t *get_render_buffer()
{
    return buf[render_buffer];
}

uint8_t *get_display_buffer()
{
    return buf[render_buffer ^ 1];
}
