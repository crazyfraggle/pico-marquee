// This is kind of mirroring the pixels.h API
//
// #define WIDTH 128
// #define HEIGHT 32

// #define PIXEL_RED(buf, x, y) buf[((x) + (y)*WIDTH) * 3]
// #define PIXEL_GRN(buf, x, y) buf[1 + ((x) + ((y)*WIDTH)) * 3]
// #define PIXEL_BLU(buf, x, y) buf[2 + ((x) + ((y)*WIDTH)) * 3]

// static uint32_t r_ish;
// #define rnd (r_ish = (((r_ish * 1103515245) + 12345) & 0x7fffffff))

// // Prototypes
// uint32_t rgb(uint8_t r, uint8_t g, uint8_t b);
// void set_pixel(int x, int y, uint32_t p);
// uint8_t *get_display_buffer();
// uint8_t *get_render_buffer();
// void flip_buffer(bool copy);
// void clear_buffers();
// int font_print_string(int x, int y, uint32_t color, char *string);

export const WIDTH = 128;
export const HEIGHT = 32;
export const rnd = () => Math.abs(Math.random() * 0x7fffffff) | 0;

export const RED = (x: number, y: number) => 0 + (x + y * WIDTH) * 3;
export const GRN = (x: number, y: number) => 1 + (x + y * WIDTH) * 3;
export const BLU = (x: number, y: number) => 2 + (x + y * WIDTH) * 3;

// The buffers, 1 for rendering, 1 for display
const buf = [new Uint8Array(WIDTH * (HEIGHT + 1) * 3), new Uint8Array(WIDTH * (HEIGHT + 1) * 3)];
let render_buffer = 0;

export const rgb = (r: number, g: number, b: number) => (b << 16) | (g << 8) | r;

export const set_pixel = (x: number, y: number, bgr: number) => {
  buf[render_buffer][RED(x, y)] = bgr & 0xff;
  buf[render_buffer][GRN(x, y)] = (bgr >> 8) & 0xff;
  buf[render_buffer][BLU(x, y)] = (bgr >> 16) & 0xff;
};

export const get_display_buffer = () => buf[render_buffer ^ 1];

export const get_render_buffer = () => buf[render_buffer];

export const flip_buffer = (copy: boolean) => {
  render_buffer ^= 1;
  if (copy) {
    // memcpy(buf[render_buffer], buf[render_buffer ^ 1], HEIGHT * WIDTH * 3);
    buf[render_buffer] = new Uint8Array(buf[render_buffer ^ 1]);
  }
};

export const clear_buffers = () => {
  buf[0].fill(0);
  buf[1].fill(0);
};

export const onScreen = (x: number, y: number) => x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
