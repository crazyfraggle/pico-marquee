#ifndef PIXELS_H
#define PIXELS_H

#include <stdlib.h>
#include "pico/stdlib.h"

#define WIDTH 128
#define HEIGHT 32

#define RED(x, y) (0 + ((x) + ((y)*WIDTH)) * 3)
#define GRN(x, y) (1 + ((x) + ((y)*WIDTH)) * 3)
#define BLU(x, y) (2 + ((x) + ((y)*WIDTH)) * 3)
#define PIXEL_RED(buf, x, y) buf[RED(x, y)]
#define PIXEL_GRN(buf, x, y) buf[GRN(x, y)]
#define PIXEL_BLU(buf, x, y) buf[BLU(x, y)]

static uint32_t r_ish;
#define rnd (r_ish = (((r_ish * 1103515245) + 12345) & 0x7fffffff))

// Prototypes
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
void set_pixel(int x, int y, uint32_t p);
uint8_t *get_display_buffer();
uint8_t *get_render_buffer();
void flip_buffer(bool copy);
void clear_buffers();
int font_print_string(int x, int y, uint32_t color, char *string);

#endif