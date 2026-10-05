/*
 * Sine scroller, as in the C64 demos: the text enters on the right and
 * every pixel column is lifted by a sine wave that also rolls along, so the
 * letters ride a moving wave across the panel.
 */

#include <math.h>
#include <string.h>

#include "pixels.h"
#include "text.h"
#include "demos/scroller.h"

// Wave height in pixels either side of the centre line.
#define WAVE_AMPLITUDE 9
// Sine table steps per screen column; 256 steps is one full period.
#define WAVE_STRETCH 2
// Sine table steps the wave rolls each frame.
#define WAVE_SPEED 3

static char text[SCROLLER_MAX_LEN + 1] = "THIS IS A CLASSIC SCROLL TEXT DEMO FROM C64! ";
static uint32_t colour = 0xffffff;

static int8_t sine[256];
static int scroll_x;
static uint8_t phase;

void init_scroller(void)
{
    for (int i = 0; i < 256; i++)
    {
        sine[i] = (int8_t)lroundf(127.0f * sinf(i * 6.2831853f / 256.0f));
    }

    scroll_x = WIDTH;
    phase = 0;
}

bool render_scroller(void)
{
    uint8_t *buf = get_render_buffer();
    memset(buf, 0, WIDTH * HEIGHT * 3);

    int text_w = (int)strlen(text) * TEXT_CHAR_WIDTH;
    int centre = (HEIGHT - TEXT_CHAR_HEIGHT) / 2;

    for (int px = 0; px < WIDTH; px++)
    {
        int tx = px - scroll_x;
        if (tx < 0 || tx >= text_w)
        {
            continue;
        }

        uint8_t glyph = text_ascii_to_c64(text[tx / TEXT_CHAR_WIDTH]);
        int bit = 7 - tx % TEXT_CHAR_WIDTH;
        int8_t s = sine[(uint8_t)(px * WAVE_STRETCH + phase)];
        int top = centre + s * WAVE_AMPLITUDE / 127;

        for (int line = 0; line < TEXT_CHAR_HEIGHT; line++)
        {
            int py = top + line;
            if (py >= 0 && py < HEIGHT && (text_glyph_row(glyph, line) >> bit) & 1)
            {
                set_pixel(px, py, colour);
            }
        }
    }

    phase += WAVE_SPEED;
    if (--scroll_x < -text_w)
    {
        scroll_x = WIDTH;
    }

    return true;
}

bool scroller_parse_command(const char *line)
{
    const char *colon = strchr(line, ':');
    if (colon == NULL)
    {
        return false;
    }

    size_t colour_len = (size_t)(colon - line);
    uint32_t new_colour = colour;
    if (colour_len > 0)
    {
        char hex[7];
        if (colour_len != 6)
        {
            return false;
        }
        memcpy(hex, line, 6);
        hex[6] = '\0';
        if (!text_parse_hex_colour(hex, &new_colour))
        {
            return false;
        }
    }

    const char *s = colon + 1;
    if (strlen(s) > SCROLLER_MAX_LEN)
    {
        return false;
    }

    colour = new_colour;
    if (s[0] != '\0')
    {
        strcpy(text, s);
    }
    return true;
}
