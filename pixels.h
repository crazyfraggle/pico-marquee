#include <stdlib.h>
#include "pico/stdlib.h"

#define WIDTH 128
#define HEIGHT 32

// Prototypes
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
void set_pixel(int x, int y, uint32_t p);
uint8_t *get_buffer();
