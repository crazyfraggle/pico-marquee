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
export const rnd = () => (Math.random() * Number.MAX_SAFE_INTEGER) | 0

export interface DisplayApi {
    rgb: (r: number, g: number, b: number) => number;
    set_pixel: (x: number, y: number, p: number) => void;
    get_display_buffer: () => Uint8Array;
    get_render_buffer: () => Uint8Array;
    flip_buffer: (copy: boolean) => void;
    clear_buffers: () => void;
    font_print_string?: (x: number, y: number, color: number, text: string) => number;
}