/*
 * Text rendering for the LED marquee.
 *
 * Owns the bundled C64 font and the strings text mode keeps on screen. Both
 * the binary "T"/"O" commands and the ASCII "t"/"o" line form funnel into
 * text_apply(), so the two wire formats always behave identically.
 */

#include <stdlib.h>
#include <string.h>

#include "pixels.h"
#include "demos.h"
#include "text.h"
#include "c64.h"

#define TEXT_GLYPH_COUNT (sizeof(c64_font2) / TEXT_CHAR_HEIGHT)
#define TEXT_GLYPH_SPACE 32

typedef struct
{
    bool used;
    int x;
    int y;
    uint32_t fg;
    uint32_t bg;
    bool draw_bg;
    char s[TEXT_MAX_LEN + 1];
} text_line_t;

static text_line_t lines[TEXT_MAX_LINES];

uint8_t text_ascii_to_c64(char c)
{
    // The font only carries uppercase shapes.
    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - 'a' + 'A');
    }

    // '@' through 'Z' sit at the start of the C64 charmap.
    if (c >= '@' && c <= 'Z')
    {
        return (uint8_t)(c - '@');
    }

    // Space through '?' (digits and common punctuation) match ASCII.
    if (c >= ' ' && c <= '?')
    {
        return (uint8_t)c;
    }

    return TEXT_GLYPH_SPACE;
}

void text_draw_glyph(uint8_t *buf, int x, int y, uint8_t glyph,
                     uint32_t fg, uint32_t bg, bool draw_bg)
{
    if ((size_t)glyph >= TEXT_GLYPH_COUNT)
    {
        glyph = TEXT_GLYPH_SPACE;
    }

    for (int line = 0; line < TEXT_CHAR_HEIGHT; line++)
    {
        int py = y + line;
        if (py < 0 || py >= HEIGHT)
        {
            continue;
        }

        uint8_t bits = (uint8_t)c64_font2[glyph * TEXT_CHAR_HEIGHT + line];

        for (int b = 0; b < TEXT_CHAR_WIDTH; b++)
        {
            int px = x + b;
            if (px < 0 || px >= WIDTH)
            {
                continue;
            }

            bool lit = (bits >> (7 - b)) & 1;
            if (!lit && !draw_bg)
            {
                continue;
            }

            uint32_t colour = lit ? fg : bg;
            PIXEL_RED(buf, px, py) = colour & 0xff;
            PIXEL_GRN(buf, px, py) = (colour >> 8) & 0xff;
            PIXEL_BLU(buf, px, py) = (colour >> 16) & 0xff;
        }
    }
}

uint8_t text_glyph_row(uint8_t glyph, int line)
{
    if ((size_t)glyph >= TEXT_GLYPH_COUNT || line < 0 || line >= TEXT_CHAR_HEIGHT)
    {
        return 0;
    }

    return (uint8_t)c64_font2[glyph * TEXT_CHAR_HEIGHT + line];
}

int text_draw_string(uint8_t *buf, int x, int y, const char *s,
                     uint32_t fg, uint32_t bg, bool draw_bg)
{
    for (int i = 0; s[i] != '\0'; i++)
    {
        text_draw_glyph(buf, x + i * TEXT_CHAR_WIDTH, y,
                        text_ascii_to_c64(s[i]), fg, bg, draw_bg);
    }

    return x + (int)strlen(s) * TEXT_CHAR_WIDTH;
}

void text_set_line(int x, int y, const char *s,
                   uint32_t fg, uint32_t bg, bool draw_bg)
{
    text_line_t *slot = NULL;

    // Resending a string at the same y replaces that row.
    for (int i = 0; i < TEXT_MAX_LINES; i++)
    {
        if (lines[i].used && lines[i].y == y)
        {
            slot = &lines[i];
            break;
        }
    }

    if (slot == NULL)
    {
        // An empty string only clears; don't consume a slot for it.
        if (s[0] == '\0')
        {
            return;
        }

        for (int i = 0; i < TEXT_MAX_LINES; i++)
        {
            if (!lines[i].used)
            {
                slot = &lines[i];
                break;
            }
        }
    }

    // All rows taken by other y positions; drop the oldest.
    if (slot == NULL)
    {
        for (int i = 1; i < TEXT_MAX_LINES; i++)
        {
            lines[i - 1] = lines[i];
        }
        slot = &lines[TEXT_MAX_LINES - 1];
    }

    if (s[0] == '\0')
    {
        slot->used = false;
        return;
    }

    slot->used = true;
    slot->x = x;
    slot->y = y;
    slot->fg = fg;
    slot->bg = bg;
    slot->draw_bg = draw_bg;
    strncpy(slot->s, s, TEXT_MAX_LEN);
    slot->s[TEXT_MAX_LEN] = '\0';
}

void text_clear_all(void)
{
    for (int i = 0; i < TEXT_MAX_LINES; i++)
    {
        lines[i].used = false;
    }
}

bool render_textmode(void)
{
    uint8_t *buf = get_render_buffer();

    // The previous frame is copied forward by flip_buffer(true), so the
    // panel has to be wiped before the strings are laid down again.
    memset(buf, 0, WIDTH * HEIGHT * 3);

    for (int i = 0; i < TEXT_MAX_LINES; i++)
    {
        if (lines[i].used)
        {
            text_draw_string(buf, lines[i].x, lines[i].y, lines[i].s,
                             lines[i].fg, lines[i].bg, lines[i].draw_bg);
        }
    }

    return true;
}

void text_apply(int x, int y, const char *s, uint32_t fg, uint32_t bg,
                bool draw_bg, bool persistent)
{
    if (persistent)
    {
        text_set_line(x, y, s, fg, bg, draw_bg);
        select_demo(DEMO_TEXT);
        return;
    }

    // One-shot: paint straight into the frame being built. Demos that repaint
    // the whole panel will overwrite it on the next frame.
    text_draw_string(get_render_buffer(), x, y, s, fg, bg, draw_bg);
}

// Parse exactly six hex digits as rrggbb.
bool text_parse_hex_colour(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    int i = 0;

    for (; i < 6; i++)
    {
        char c = s[i];
        uint32_t digit;

        if (c >= '0' && c <= '9')
            digit = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = (uint32_t)(c - 'A' + 10);
        else
            return false;

        v = (v << 4) | digit;
    }

    if (s[i] != '\0')
    {
        return false;
    }

    *out = rgb((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
    return true;
}

bool text_parse_command(const char *line, bool persistent)
{
    // The text runs to the end of the line, so only the first colon splits.
    // That keeps ',' and ':' usable inside the text itself.
    const char *colon = strchr(line, ':');
    if (colon == NULL)
    {
        return false;
    }

    char header[32];
    size_t header_len = (size_t)(colon - line);
    if (header_len >= sizeof(header))
    {
        return false;
    }
    memcpy(header, line, header_len);
    header[header_len] = '\0';

    char *fields[4];
    int field_count = 0;
    char *p = header;
    while (field_count < 4)
    {
        fields[field_count++] = p;
        char *comma = strchr(p, ',');
        if (comma == NULL)
        {
            break;
        }
        *comma = '\0';
        p = comma + 1;
    }

    if (field_count < 3 || fields[0][0] == '\0' || fields[1][0] == '\0')
    {
        return false;
    }

    int x = atoi(fields[0]);
    int y = atoi(fields[1]);

    uint32_t fg;
    if (!text_parse_hex_colour(fields[2], &fg))
    {
        return false;
    }

    uint32_t bg = 0;
    bool draw_bg = false;
    if (field_count == 4)
    {
        if (!text_parse_hex_colour(fields[3], &bg))
        {
            return false;
        }
        draw_bg = true;
    }

    char text[TEXT_MAX_LEN + 1];
    strncpy(text, colon + 1, TEXT_MAX_LEN);
    text[TEXT_MAX_LEN] = '\0';

    text_apply(x, y, text, fg, bg, draw_bg, persistent);
    return true;
}
