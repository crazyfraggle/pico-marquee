/*
 * Morphing clock: the local time (HH:MM:SS) in large stroked digits that
 * fill the panel. Every digit is the same shape, a path of four cubic Bezier
 * segments, so a change of digit is drawn by interpolating the control points
 * of the old and new digit over MORPH_US.
 *
 * Paths are rasterised by stamping an anti-aliased round brush along them
 * into a coverage map, which is then tinted with a colour whose channels
 * drift independently between 20% and 70% brightness. There is no
 * background.
 *
 * Coordinates are fixed point in 1/16 pixel, so no floats are used per frame
 * apart from the three colour sines.
 */

#include <math.h>
#include <string.h>

#include "pico/time.h"

#include "pixels.h"
#include "clock.h"
#include "demos/morph.h"

#define FIX 16
#define P(x, y) {(int16_t)((x) * FIX), (int16_t)((y) * FIX)}

// Four cubic segments share their end points: 1 + 4 * 3 control points.
#define SEGMENTS 4
#define POINTS (1 + SEGMENTS * 3)
// Brush stamps per segment. Segments are at most ~22 px long, so this keeps
// stamps well under a pixel apart.
#define STEPS 32

#define MORPH_US 750000

// Brush radius in pixels, plus half a pixel of anti-aliasing either side.
#define BRUSH_RADIUS 1.5f
#define BRUSH_REACH 2
#define BRUSH_SIZE (2 * BRUSH_REACH + 1)
// Brush positions are quantised to quarter pixels.
#define BRUSH_SUB 4

// Colour channels swing between these levels, each with its own period.
#define LEVEL_MIN 51  // 20%
#define LEVEL_MAX 178 // 70%
#define PERIOD_R_US 23000000ull
#define PERIOD_G_US 37000000ull
#define PERIOD_B_US 53000000ull

typedef struct
{
    int16_t x, y;
} point_t;

// Glyph paths in a 13 x 27 pixel box, measured on the stroke centre line.
// 10 is the dash shown before the clock is set.
#define GLYPH_DASH 10
static const point_t glyphs[11][POINTS] = {
    // 0: an ellipse, clockwise from the top.
    {P(6.5, 0), P(10.09, 0), P(13, 6.04), P(13, 13.5),
     P(13, 20.96), P(10.09, 27), P(6.5, 27),
     P(2.91, 27), P(0, 20.96), P(0, 13.5),
     P(0, 6.04), P(2.91, 0), P(6.5, 0)},
    // 1: a flag, then the stem in three straight pieces.
    {P(2.5, 5), P(4.5, 4), P(6.5, 2), P(7.5, 0),
     P(7.5, 3), P(7.5, 6), P(7.5, 9),
     P(7.5, 12), P(7.5, 15), P(7.5, 18),
     P(7.5, 21), P(7.5, 24), P(7.5, 27)},
    // 2
    {P(0.5, 6.5), P(0.5, 2.7), P(3.3, 0), P(6.5, 0),
     P(10, 0), P(12.5, 3), P(12.5, 7),
     P(12.5, 12), P(5, 17), P(0, 27),
     P(4.33, 27), P(8.67, 27), P(13, 27)},
    // 3: a small bowl over a larger one.
    {P(0.5, 4), P(2, 1.2), P(4.2, 0), P(6.5, 0),
     P(14, 0), P(12.5, 13), P(5, 13),
     P(13.5, 13), P(14, 27), P(6.5, 27),
     P(4, 27), P(1.8, 25.8), P(0.5, 23)},
    // 4: crossbar, diagonal, then the stem.
    {P(13, 19), P(8.67, 19), P(4.33, 19), P(0, 19),
     P(3.17, 12.67), P(6.33, 6.33), P(9.5, 0),
     P(9.5, 4.5), P(9.5, 9), P(9.5, 13.5),
     P(9.5, 18), P(9.5, 22.5), P(9.5, 27)},
    // 5
    {P(12, 0), P(8.5, 0), P(5, 0), P(1.5, 0),
     P(1.17, 4), P(0.83, 8), P(0.5, 12),
     P(4, 9.5), P(12.5, 9.5), P(12.5, 19),
     P(12.5, 28.5), P(3, 28.5), P(0.5, 24)},
    // 6
    {P(11, 1.5), P(8, -1), P(0, 2), P(0, 16),
     P(0, 23), P(3, 27), P(6.5, 27),
     P(14, 27), P(14, 11), P(6.5, 11),
     P(3, 11), P(0.5, 13.5), P(0.3, 17)},
    // 7: the top bar in two pieces, then a gently curved stem.
    {P(0, 0), P(2.17, 0), P(4.33, 0), P(6.5, 0),
     P(8.67, 0), P(10.83, 0), P(13, 0),
     P(11.3, 4.3), P(9.5, 8.5), P(8, 13),
     P(6.8, 16.8), P(5.5, 21.5), P(4.5, 27)},
    // 8: two stacked loops crossing in the middle.
    {P(6.5, 12.5), P(0.5, 11), P(0.5, 0), P(6.5, 0),
     P(12.5, 0), P(12.5, 11), P(6.5, 12.5),
     P(0, 14), P(0, 27), P(6.5, 27),
     P(13, 27), P(13, 14), P(6.5, 12.5)},
    // 9: the 6 turned upside down.
    {P(2, 25.5), P(5, 28), P(13, 25), P(13, 11),
     P(13, 4), P(10, 0), P(6.5, 0),
     P(-1, 0), P(-1, 16), P(6.5, 16),
     P(10, 16), P(12.5, 13.5), P(12.7, 10)},
    // Dash
    {P(2, 13.5), P(2.75, 13.5), P(3.5, 13.5), P(4.25, 13.5),
     P(5, 13.5), P(5.75, 13.5), P(6.5, 13.5),
     P(7.25, 13.5), P(8, 13.5), P(8.75, 13.5),
     P(9.5, 13.5), P(10.25, 13.5), P(11, 13.5)},
};

// Top-left of each digit's glyph box, and the colon centres. The digits are
// 16 px wide including the stroke, 3 px apart within a pair and 8 px apart
// around a colon, leaving a 3.5 px margin either side.
static const point_t digit_pos[6] = {
    P(5, 2.5), P(24, 2.5), P(48, 2.5), P(67, 2.5), P(91, 2.5), P(110, 2.5)};
static const int16_t colon_x[2] = {(int16_t)(42.5 * FIX), (int16_t)(85.5 * FIX)};
static const int16_t colon_y[2] = {(int16_t)(10.5 * FIX), (int16_t)(21.5 * FIX)};

typedef struct
{
    uint8_t from, to;
    uint64_t start_us;
} morph_t;

static morph_t digits[6];
static uint8_t coverage[HEIGHT][WIDTH];
static uint8_t brush[BRUSH_SUB][BRUSH_SUB][BRUSH_SIZE][BRUSH_SIZE];
static bool brush_ready;

static void build_brush(void)
{
    for (int sy = 0; sy < BRUSH_SUB; sy++)
        for (int sx = 0; sx < BRUSH_SUB; sx++)
        {
            // Brush centre inside its pixel, sampled at quarter-pixel centres.
            float cx = (sx + 0.5f) / BRUSH_SUB;
            float cy = (sy + 0.5f) / BRUSH_SUB;
            for (int dy = 0; dy < BRUSH_SIZE; dy++)
                for (int dx = 0; dx < BRUSH_SIZE; dx++)
                {
                    float px = dx - BRUSH_REACH + 0.5f - cx;
                    float py = dy - BRUSH_REACH + 0.5f - cy;
                    float c = BRUSH_RADIUS + 0.5f - sqrtf(px * px + py * py);
                    if (c < 0.0f)
                        c = 0.0f;
                    if (c > 1.0f)
                        c = 1.0f;
                    brush[sy][sx][dy][dx] = (uint8_t)(c * 255.0f + 0.5f);
                }
        }
    brush_ready = true;
}

// Stamp the brush centred on (x, y) in 1/16 pixel, keeping the brighter
// coverage where it overlaps earlier stamps.
static void stamp(int x, int y)
{
    int ix = x >> 4, iy = y >> 4;
    const uint8_t(*b)[BRUSH_SIZE] = brush[(y & 15) / (FIX / BRUSH_SUB)][(x & 15) / (FIX / BRUSH_SUB)];

    for (int dy = 0; dy < BRUSH_SIZE; dy++)
    {
        int py = iy + dy - BRUSH_REACH;
        if (py < 0 || py >= HEIGHT)
            continue;
        for (int dx = 0; dx < BRUSH_SIZE; dx++)
        {
            int px = ix + dx - BRUSH_REACH;
            if (px < 0 || px >= WIDTH)
                continue;
            if (b[dy][dx] > coverage[py][px])
                coverage[py][px] = b[dy][dx];
        }
    }
}

static void draw_path(const point_t *p, int ox, int oy)
{
    for (int s = 0; s < SEGMENTS; s++, p += 3)
    {
        // Cubic Bezier with t = i / STEPS, scaled by STEPS^3 to stay integer.
        // Points are glyph-relative (under 15 px = 240), so the largest sum
        // is about 240 * 32^3, well inside int32_t.
        for (int i = (s == 0 ? 0 : 1); i <= STEPS; i++)
        {
            int t = i, u = STEPS - i;
            int a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
            int x = (a * p[0].x + b * p[1].x + c * p[2].x + d * p[3].x) / (STEPS * STEPS * STEPS);
            int y = (a * p[0].y + b * p[1].y + c * p[2].y + d * p[3].y) / (STEPS * STEPS * STEPS);
            stamp(ox + x, oy + y);
        }
    }
}

static void set_target(morph_t *m, uint8_t glyph, uint64_t now)
{
    if (glyph == m->to)
        return;
    // A change during a morph (only when the clock is set) restarts from the
    // previous target rather than the half-drawn shape.
    m->from = m->to;
    m->to = glyph;
    m->start_us = now;
}

static void draw_digit(const morph_t *m, const point_t *pos, uint64_t now)
{
    uint64_t elapsed = now - m->start_us;
    if (m->from == m->to || elapsed >= MORPH_US)
    {
        draw_path(glyphs[m->to], pos->x, pos->y);
        return;
    }

    // Smoothstep ease-in-out, with t in 0..256.
    int t = (int)(elapsed * 256 / MORPH_US);
    int e = t * t * (3 * 256 - 2 * t) / (256 * 256);

    point_t path[POINTS];
    const point_t *a = glyphs[m->from], *b = glyphs[m->to];
    for (int i = 0; i < POINTS; i++)
    {
        path[i].x = (int16_t)(a[i].x + (b[i].x - a[i].x) * e / 256);
        path[i].y = (int16_t)(a[i].y + (b[i].y - a[i].y) * e / 256);
    }
    draw_path(path, pos->x, pos->y);
}

static uint8_t channel_level(uint64_t now, uint64_t period)
{
    float phase = (float)(now % period) / (float)period * 6.2831853f;
    float level = (LEVEL_MIN + LEVEL_MAX) / 2.0f + (LEVEL_MAX - LEVEL_MIN) / 2.0f * sinf(phase);
    return (uint8_t)(level + 0.5f);
}

void init_morph(void)
{
    if (!brush_ready)
        build_brush();

    // Start on dashes; the first frame morphs straight to the time if known.
    for (int i = 0; i < 6; i++)
    {
        digits[i].from = digits[i].to = GLYPH_DASH;
        digits[i].start_us = 0;
    }
}

bool render_morph(void)
{
    uint8_t *buf = get_render_buffer();
    uint64_t now = time_us_64();
    int h, m, s;

    if (clock_local_time(&h, &m, &s))
    {
        const uint8_t glyph[6] = {h / 10, h % 10, m / 10, m % 10, s / 10, s % 10};
        for (int i = 0; i < 6; i++)
            set_target(&digits[i], glyph[i], now);
    }

    memset(coverage, 0, sizeof(coverage));
    for (int i = 0; i < 6; i++)
        draw_digit(&digits[i], &digit_pos[i], now);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            stamp(colon_x[i], colon_y[j]);

    uint8_t r = channel_level(now, PERIOD_R_US);
    uint8_t g = channel_level(now, PERIOD_G_US);
    uint8_t b = channel_level(now, PERIOD_B_US);

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            int c = coverage[y][x];
            PIXEL_RED(buf, x, y) = (uint8_t)((c * r) / 255);
            PIXEL_GRN(buf, x, y) = (uint8_t)((c * g) / 255);
            PIXEL_BLU(buf, x, y) = (uint8_t)((c * b) / 255);
        }
    }

    return true;
}
