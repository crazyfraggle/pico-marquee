#ifndef TEXT_H
#define TEXT_H

#include <stdbool.h>
#include <stdint.h>

#define TEXT_CHAR_WIDTH 8
#define TEXT_CHAR_HEIGHT 8

// Longest string a single command may carry. The display fits 16 characters
// across, but longer strings are accepted and clipped so a string can be
// positioned partly off-screen.
#define TEXT_MAX_LEN 32

// Number of strings text mode keeps. The panel is four 8 pixel rows tall.
#define TEXT_MAX_LINES 4

// Map an ASCII character onto the C64 charmap the bundled font uses.
// Lowercase folds to uppercase; anything unsupported becomes a space.
uint8_t text_ascii_to_c64(char c);

// Draw one glyph by charmap index. Pixels outside the panel are dropped.
// When draw_bg is false the unlit pixels of the glyph are left untouched.
void text_draw_glyph(uint8_t *buf, int x, int y, uint8_t glyph,
                     uint32_t fg, uint32_t bg, bool draw_bg);

// One row of a glyph's bitmap; bit 7 is the leftmost pixel.
uint8_t text_glyph_row(uint8_t glyph, int line);

// Parse exactly six hex digits as rrggbb into an rgb() colour.
bool text_parse_hex_colour(const char *s, uint32_t *out);

// Draw an ASCII string. Returns the x coordinate just past the last glyph.
int text_draw_string(uint8_t *buf, int x, int y, const char *s,
                     uint32_t fg, uint32_t bg, bool draw_bg);

// Store a string in text mode. A string already stored at the same y is
// replaced, so a row can be updated by resending it. An empty string clears
// the row.
void text_set_line(int x, int y, const char *s,
                   uint32_t fg, uint32_t bg, bool draw_bg);

// Forget every stored string.
void text_clear_all(void);

// Draw the stored strings. Called each frame while text mode is selected.
bool render_textmode(void);

// Either store the string (persistent, switching to text mode) or draw it
// once into the current render buffer.
void text_apply(int x, int y, const char *s, uint32_t fg, uint32_t bg,
                bool draw_bg, bool persistent);

// Parse "x,y,rrggbb[,rrggbb]:text" and apply it. Returns false if malformed.
bool text_parse_command(const char *line, bool persistent);

#endif /* TEXT_H */
