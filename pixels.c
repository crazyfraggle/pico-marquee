
#include "pixels.h"

// Statically allocate the shared buffer.
static uint8_t buf1[HEIGHT * WIDTH * 3];

// Create RBG 32 bit from R, G and B bytes.
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return r + (g * 256) + (b * 256 * 256);
}

void set_pixel(int x, int y, uint32_t p)
{
    buf1[0 + (x + (y * WIDTH)) * 3] = p & 0xff;
    buf1[1 + (x + (y * WIDTH)) * 3] = (p >> 8) & 0xff;
    buf1[2 + (x + (y * WIDTH)) * 3] = (p >> 16) & 0xff;
}

uint8_t *get_buffer()
{
    return buf1;
}