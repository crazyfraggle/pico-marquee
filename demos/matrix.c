/*
 * Matrix clock: falling green drops with fading trails, and the local time
 * in the C64 font drifting slowly just below the centre of the panel.
 *
 * The rain lives in its own intensity map rather than the framebuffer, so the
 * clock can be drawn on top each frame without smearing into the trails.
 */

#include <math.h>
#include <stdio.h>

#include "pico/time.h"

#include "pixels.h"
#include "text.h"
#include "clock.h"
#include "demos/matrix.h"

// Drop positions are in 1/16 pixel steps.
#define SUBPIXEL 16
// Trail brightness is multiplied by TRAIL_DECAY/256 every frame.
#define TRAIL_DECAY 218
// Frames a column stays empty before the next drop.
#define RESPAWN_MAX_WAIT 60

// The rain colour at full brightness; trails are scaled from it.
#define RAIN_R 24
#define RAIN_G 255
#define RAIN_B 64

typedef struct
{
    int16_t y;     // head position in subpixels
    uint8_t speed; // subpixels per frame
    int16_t wait;  // frames until the drop starts falling
} drop_t;

static uint8_t glow[HEIGHT][WIDTH];
static drop_t drops[WIDTH];

static void spawn_drop(drop_t *d, int max_wait)
{
    d->y = -(int16_t)(rnd % (8 * SUBPIXEL));
    d->speed = 4 + rnd % 13; // 0.25 to 1 pixel per frame
    d->wait = max_wait > 0 ? rnd % max_wait : 0;
}

void init_matrix(void)
{
    r_ish = time_us_32();

    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            glow[y][x] = 0;

    // Start some drops part way down so the panel is not empty at first.
    for (int x = 0; x < WIDTH; x++)
    {
        spawn_drop(&drops[x], RESPAWN_MAX_WAIT);
        if (rnd % 3 == 0)
        {
            drops[x].y = (int16_t)(rnd % (HEIGHT * SUBPIXEL));
            drops[x].wait = 0;
        }
    }
}

static void update_rain(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            glow[y][x] = (uint8_t)((glow[y][x] * TRAIL_DECAY) >> 8);

    for (int x = 0; x < WIDTH; x++)
    {
        drop_t *d = &drops[x];

        if (d->wait > 0)
        {
            d->wait--;
            continue;
        }

        // Light every row the head passed this frame, so the trail has no
        // gaps when a drop moves more than a pixel.
        int from = d->y / SUBPIXEL;
        d->y += d->speed;
        int to = d->y / SUBPIXEL;
        for (int y = from; y <= to; y++)
        {
            if (y >= 0 && y < HEIGHT)
                glow[y][x] = 255;
        }

        if (to >= HEIGHT)
            spawn_drop(d, RESPAWN_MAX_WAIT);
    }
}

static void draw_rain(uint8_t *buf)
{
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            uint8_t g = glow[y][x];
            PIXEL_RED(buf, x, y) = (uint8_t)((g * RAIN_R) >> 8);
            PIXEL_GRN(buf, x, y) = (uint8_t)((g * RAIN_G) >> 8);
            PIXEL_BLU(buf, x, y) = (uint8_t)((g * RAIN_B) >> 8);
        }
    }

    // Leading pixels glow nearly white, as in the film.
    for (int x = 0; x < WIDTH; x++)
    {
        int y = drops[x].y / SUBPIXEL;
        if (drops[x].wait == 0 && y >= 0 && y < HEIGHT)
        {
            PIXEL_RED(buf, x, y) = 170;
            PIXEL_GRN(buf, x, y) = 255;
            PIXEL_BLU(buf, x, y) = 170;
        }
    }
}

static void draw_clock(uint8_t *buf)
{
    char s[9];
    int h, m, sec;

    if (clock_local_time(&h, &m, &sec))
        snprintf(s, sizeof(s), "%02d:%02d:%02d", h, m, sec);
    else
        snprintf(s, sizeof(s), "--:--:--");

    // Drift slowly along a Lissajous path centred a little below the middle
    // of the panel. At 25 fps the periods are ~40 s and ~27 s, so the path
    // rarely repeats. The phases wrap so they never lose float precision.
    static float phase_x = 0.0f, phase_y = 0.0f;
    const float two_pi = 6.2831853f;
    phase_x += 0.00628f;
    phase_y += 0.00931f;
    if (phase_x >= two_pi)
        phase_x -= two_pi;
    if (phase_y >= two_pi)
        phase_y -= two_pi;

    const int text_w = 8 * TEXT_CHAR_WIDTH;
    int x = (WIDTH - text_w) / 2 + (int)lroundf(24.0f * sinf(phase_x));
    int y = 14 + (int)lroundf(4.0f * sinf(phase_y));

    // A black outline keeps the digits readable over bright trails.
    uint32_t black = rgb(0, 0, 0);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx != 0 || dy != 0)
                text_draw_string(buf, x + dx, y + dy, s, black, black, false);

    text_draw_string(buf, x, y, s, rgb(0xcc, 0xcc, 0xcc), black, false);
}

bool render_matrix(void)
{
    uint8_t *buf = get_render_buffer();

    update_rain();
    draw_rain(buf);
    draw_clock(buf);

    return true;
}
