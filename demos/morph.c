/*
 * Morphing clock: the local time (HH:MM:SS) in large stroked digits that
 * fill the panel. Every digit is the same shape, a path of four cubic Bezier
 * segments, so a change of digit is drawn by interpolating the control points
 * of the old and new digit over MORPH_US. Changed digits start 50 ms apart
 * from right to left, with a randomly chosen easing for each path point.
 *
 * Paths are rasterised by stamping an anti-aliased round brush along them
 * into a coverage map, which is then tinted with a colour whose channels
 * drift independently between 20% and 70% brightness. Small fireworks play
 * behind the digits at the top of each hour.
 *
 * Coordinates are fixed point in 1/16 pixel, so no floats are used per frame
 * apart from the three colour sines. Firework velocities are calculated
 * once when the show starts; their per-frame trajectories stay integer-only.
 */

#include <math.h>
#include <string.h>

#include "pico/time.h"

#include "clock.h"
#include "demos/morph.h"
#include "pixels.h"

#define FIX 16
#define P(x, y) {(int16_t)((x) * FIX), (int16_t)((y) * FIX)}

// Four cubic segments share their end points: 1 + 4 * 3 control points.
#define SEGMENTS 4
#define POINTS (1 + SEGMENTS * 3)
// Brush stamps per segment. Segments are at most ~22 px long, so this keeps
// stamps well under a pixel apart.
#define STEPS 32

#define MORPH_US 750000
#define DIGIT_STAGGER_US 50000
#define TRANSITION_CURVES 6
#define EASE_STEPS 256
#define EASE_SCALE 65535

// Brush radius in pixels, plus half a pixel of anti-aliasing either side.
#define BRUSH_RADIUS 1.2f
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

typedef struct {
    int16_t x, y;
} point_t;

// Glyph paths in a 13 x 27 pixel box, measured on the stroke centre line.
// 10 is the dash shown before the clock is set.
#define GLYPH_DASH 10
static const point_t glyphs[11][POINTS] = {
    // 0: an ellipse, clockwise from the top.
    {P(6.5, 0), P(10.09, 0), P(13, 6.04), P(13, 13.5), P(13, 20.96),
     P(10.09, 27), P(6.5, 27), P(2.91, 27), P(0, 20.96), P(0, 13.5), P(0, 6.04),
     P(2.91, 0), P(6.5, 0)},
    // 1: a flag, then the stem in three straight pieces.
    {P(2.5, 5), P(4.5, 4), P(6.5, 2), P(7.5, 0), P(7.5, 3), P(7.5, 6),
     P(7.5, 9), P(7.5, 12), P(7.5, 15), P(7.5, 18), P(7.5, 21), P(7.5, 24),
     P(7.5, 27)},
    // 2
    {P(0.5, 6.5), P(0.5, 2.7), P(3.3, 0), P(6.5, 0), P(10, 0), P(12.5, 3),
     P(12.5, 7), P(12.5, 12), P(5, 17), P(0, 27), P(4.33, 27), P(8.67, 27),
     P(13, 27)},
    // 3: a small bowl over a larger one.
    {P(0.5, 4), P(2, 1.2), P(4.2, 0), P(6.5, 0), P(14, 0), P(12.5, 13),
     P(5, 13), P(13.5, 13), P(14, 27), P(6.5, 27), P(4, 27), P(1.8, 25.8),
     P(0.5, 23)},
    // 4: crossbar, diagonal, then the stem.
    {P(13, 19), P(8.67, 19), P(4.33, 19), P(0, 19), P(3.17, 12.67),
     P(6.33, 6.33), P(9.5, 0), P(9.5, 4.5), P(9.5, 9), P(9.5, 13.5), P(9.5, 18),
     P(9.5, 22.5), P(9.5, 27)},
    // 5
    {P(12, 0), P(8.5, 0), P(5, 0), P(1.5, 0), P(1.17, 4), P(0.83, 8),
     P(0.5, 12), P(4, 9.5), P(12.5, 9.5), P(12.5, 19), P(12.5, 28.5),
     P(3, 28.5), P(0.5, 24)},
    // 6
    {P(11, 1.5), P(8, -1), P(0, 2), P(0, 16), P(0, 23), P(3, 27), P(6.5, 27),
     P(14, 27), P(14, 11), P(6.5, 11), P(3, 11), P(0.5, 13.5), P(0.3, 17)},
    // 7: the top bar in two pieces, then a gently curved stem.
    {P(0, 0), P(2.17, 0), P(4.33, 0), P(6.5, 0), P(8.67, 0), P(10.83, 0),
     P(13, 0), P(11.3, 4.3), P(9.5, 8.5), P(8, 13), P(6.8, 16.8), P(5.5, 21.5),
     P(4.5, 27)},
    // 8: two stacked loops crossing in the middle.
    {P(6.5, 12.5), P(0.5, 11), P(0.5, 0), P(6.5, 0), P(12.5, 0), P(12.5, 11),
     P(6.5, 12.5), P(0, 14), P(0, 27), P(6.5, 27), P(13, 27), P(13, 14),
     P(6.5, 12.5)},
    // 9: the 6 turned upside down.
    {P(2, 25.5), P(5, 28), P(13, 25), P(13, 11), P(13, 4), P(10, 0), P(6.5, 0),
     P(-1, 0), P(-1, 16), P(6.5, 16), P(10, 16), P(12.5, 13.5), P(12.7, 10)},
    // Dash
    {P(2, 13.5), P(2.75, 13.5), P(3.5, 13.5), P(4.25, 13.5), P(5, 13.5),
     P(5.75, 13.5), P(6.5, 13.5), P(7.25, 13.5), P(8, 13.5), P(8.75, 13.5),
     P(9.5, 13.5), P(10.25, 13.5), P(11, 13.5)},
};

// Top-left of each digit's glyph box, and the colon centres. The digits are
// 16 px wide including the stroke, 3 px apart within a pair and 8 px apart
// around a colon, leaving a 3.5 px margin either side.
static const point_t digit_pos[6] = {P(5, 2.5),  P(24, 2.5), P(48, 2.5),
                                     P(67, 2.5), P(91, 2.5), P(110, 2.5)};
static const int16_t colon_x[2] = {(int16_t)(42.5 * FIX),
                                   (int16_t)(85.5 * FIX)};
static const int16_t colon_y[2] = {(int16_t)(10.5 * FIX),
                                   (int16_t)(21.5 * FIX)};

typedef struct {
    uint8_t from, to;
    uint64_t start_us;
    uint8_t point_transitions[POINTS];
} morph_t;

static morph_t digits[6];
static uint8_t coverage[HEIGHT][WIDTH];
static uint8_t brush[BRUSH_SUB][BRUSH_SUB][BRUSH_SIZE][BRUSH_SIZE];
static uint16_t easing[TRANSITION_CURVES][EASE_STEPS + 1];
static bool brush_ready;

#define FIREWORKS 5
#define SPARKS 18
#define LAUNCH_MS 500
#define BURST_MS 1600
#define FIREWORK_GAP_MS 900
#define FIREWORK_DURATION_MS                                                   \
    ((FIREWORKS - 1) * FIREWORK_GAP_MS + LAUNCH_MS + BURST_MS)

typedef struct {
    point_t centre;
    point_t velocity[SPARKS];
    uint8_t colour;
} firework_t;

static const uint8_t firework_colours[][3] = {{140, 42, 18},
                                              {28, 100, 150},
                                              {120, 35, 135},
                                              {140, 105, 20},
                                              {35, 135, 70}};
static firework_t fireworks[FIREWORKS];
static uint64_t fireworks_start;
static bool fireworks_active;
static int last_clock_second = -1;

static void start_fireworks(uint64_t now) {
    fireworks_start = now;
    fireworks_active = true;
    for (int i = 0; i < FIREWORKS; i++) {
        firework_t *f = &fireworks[i];
        f->centre.x = (12 + rnd % (WIDTH - 24)) * FIX;
        f->centre.y = (6 + rnd % 10) * FIX;
        f->colour =
            rnd % (sizeof(firework_colours) / sizeof(firework_colours[0]));
        float phase = (rnd % 6283) / 1000.0f;
        for (int j = 0; j < SPARKS; j++) {
            float angle = phase + j * 6.2831853f / SPARKS;
            int speed = (8 + rnd % 9) * FIX;
            f->velocity[j].x = (int16_t)(cosf(angle) * speed);
            f->velocity[j].y = (int16_t)(sinf(angle) * speed);
        }
    }
}

static void firework_pixel(uint8_t *buf, int x, int y, const uint8_t *colour,
                           int brightness) {
    if (x < 0 || y < 0)
        return;
    x /= FIX;
    y /= FIX;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
        return;
    for (int channel = 0; channel < 3; channel++) {
        int index = RED(x, y) + channel;
        int value = colour[channel] * brightness / 255;
        if (value > buf[index])
            buf[index] = (uint8_t)value;
    }
}

static void draw_fireworks(uint8_t *buf, uint64_t now) {
    if (!fireworks_active)
        return;
    uint64_t elapsed = (now - fireworks_start) / 1000;
    if (elapsed >= FIREWORK_DURATION_MS) {
        fireworks_active = false;
        return;
    }
    for (int i = 0; i < FIREWORKS; i++) {
        int age = (int)elapsed - i * FIREWORK_GAP_MS;
        if (age < 0 || age >= LAUNCH_MS + BURST_MS)
            continue;
        const firework_t *f = &fireworks[i];
        const uint8_t *colour = firework_colours[f->colour];
        if (age < LAUNCH_MS) {
            int y = (HEIGHT - 1) * FIX +
                    (f->centre.y - (HEIGHT - 1) * FIX) * age / LAUNCH_MS;
            firework_pixel(buf, f->centre.x, y, colour, 220);
            firework_pixel(buf, f->centre.x, y + FIX, colour, 90);
            continue;
        }
        int t = age - LAUNCH_MS;
        int brightness = 255 * (BURST_MS - t) / BURST_MS;
        for (int j = 0; j < SPARKS; j++)
            for (int trail = 1; trail >= 0; trail--) {
                int dt = t - trail * 70;
                if (dt < 0)
                    continue;
                int x = f->centre.x + f->velocity[j].x * dt / 1000;
                int y = f->centre.y + f->velocity[j].y * dt / 1000 +
                        8 * FIX * dt * dt / 1000000;
                firework_pixel(buf, x, y, colour, brightness / (trail + 1));
            }
    }
}

static void build_easing(void) {
    for (int i = 0; i <= EASE_STEPS; i++) {
        float t = (float)i / EASE_STEPS;
        const float warped[3] = {t * sqrtf(t), t * t, t * t * t};
        for (int curve = 0; curve < 3; curve++) {
            float u = warped[curve];
            uint16_t e =
                (uint16_t)(u * u * (3.0f - 2.0f * u) * EASE_SCALE + 0.5f);
            easing[curve][i] = e;
            easing[curve + 3][EASE_STEPS - i] = EASE_SCALE - e;
        }
    }
}

static void build_brush(void) {
    for (int sy = 0; sy < BRUSH_SUB; sy++)
        for (int sx = 0; sx < BRUSH_SUB; sx++) {
            // Brush centre inside its pixel, sampled at quarter-pixel centres.
            float cx = (sx + 0.5f) / BRUSH_SUB;
            float cy = (sy + 0.5f) / BRUSH_SUB;
            for (int dy = 0; dy < BRUSH_SIZE; dy++)
                for (int dx = 0; dx < BRUSH_SIZE; dx++) {
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
static void stamp(int x, int y) {
    int ix = x >> 4, iy = y >> 4;
    const uint8_t (*b)[BRUSH_SIZE] =
        brush[(y & 15) / (FIX / BRUSH_SUB)][(x & 15) / (FIX / BRUSH_SUB)];

    for (int dy = 0; dy < BRUSH_SIZE; dy++) {
        int py = iy + dy - BRUSH_REACH;
        if (py < 0 || py >= HEIGHT)
            continue;
        for (int dx = 0; dx < BRUSH_SIZE; dx++) {
            int px = ix + dx - BRUSH_REACH;
            if (px < 0 || px >= WIDTH)
                continue;
            if (b[dy][dx] > coverage[py][px])
                coverage[py][px] = b[dy][dx];
        }
    }
}

static void draw_path(const point_t *p, int ox, int oy) {
    for (int s = 0; s < SEGMENTS; s++, p += 3) {
        // Cubic Bezier with t = i / STEPS, scaled by STEPS^3 to stay integer.
        // Points are glyph-relative (under 15 px = 240), so the largest sum
        // is about 240 * 32^3, well inside int32_t.
        for (int i = (s == 0 ? 0 : 1); i <= STEPS; i++) {
            int t = i, u = STEPS - i;
            int a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t,
                d = t * t * t;
            int x = (a * p[0].x + b * p[1].x + c * p[2].x + d * p[3].x) /
                    (STEPS * STEPS * STEPS);
            int y = (a * p[0].y + b * p[1].y + c * p[2].y + d * p[3].y) /
                    (STEPS * STEPS * STEPS);
            stamp(ox + x, oy + y);
        }
    }
}

static void set_target(morph_t *m, uint8_t glyph, uint64_t now) {
    if (glyph == m->to)
        return;
    // A change during a morph (only when the clock is set) restarts from the
    // previous target rather than the half-drawn shape.
    m->from = m->to;
    m->to = glyph;
    m->start_us = now;
    for (int i = 0; i < POINTS; i++)
        m->point_transitions[i] = rnd % TRANSITION_CURVES;

    // Keep the duplicated endpoints together when both glyphs are closed.
    const point_t *a = glyphs[m->from], *b = glyphs[m->to];
    if (a[0].x == a[POINTS - 1].x && a[0].y == a[POINTS - 1].y &&
        b[0].x == b[POINTS - 1].x && b[0].y == b[POINTS - 1].y)
        m->point_transitions[POINTS - 1] = m->point_transitions[0];
}

static void draw_digit(const morph_t *m, const point_t *pos, uint64_t now) {
    // Check before subtracting: staggered start times can be in the future.
    if (now <= m->start_us) {
        draw_path(glyphs[m->from], pos->x, pos->y);
        return;
    }
    uint64_t elapsed = now - m->start_us;
    if (m->from == m->to || elapsed >= MORPH_US) {
        draw_path(glyphs[m->to], pos->x, pos->y);
        return;
    }

    int t = (int)(elapsed * EASE_STEPS / MORPH_US);

    point_t path[POINTS];
    const point_t *a = glyphs[m->from], *b = glyphs[m->to];
    for (int i = 0; i < POINTS; i++) {
        int e = easing[m->point_transitions[i]][t];
        path[i].x = (int16_t)(a[i].x + (b[i].x - a[i].x) * e / EASE_SCALE);
        path[i].y = (int16_t)(a[i].y + (b[i].y - a[i].y) * e / EASE_SCALE);
    }
    draw_path(path, pos->x, pos->y);
}

static uint8_t channel_level(uint64_t now, uint64_t period) {
    float phase = (float)(now % period) / (float)period * 6.2831853f;
    float level = (LEVEL_MIN + LEVEL_MAX) / 2.0f +
                  (LEVEL_MAX - LEVEL_MIN) / 2.0f * sinf(phase);
    return (uint8_t)(level + 0.5f);
}

void init_morph(void) {
    fireworks_active = false;
    last_clock_second = -1;
    if (!brush_ready) {
        build_brush();
        build_easing();
    }

    // Start on dashes; the first frame morphs straight to the time if known.
    for (int i = 0; i < 6; i++) {
        digits[i].from = digits[i].to = GLYPH_DASH;
        digits[i].start_us = 0;
        memset(digits[i].point_transitions, 0,
               sizeof(digits[i].point_transitions));
    }
}

bool render_morph(void) {
    uint8_t *buf = get_render_buffer();
    uint64_t now = time_us_64();
    int h, m, s;

    if (clock_local_time(&h, &m, &s)) {
        int clock_second = h * 3600 + m * 60 + s;
        if (m == 0 && s == 0 && clock_second != last_clock_second)
            start_fireworks(now);
        last_clock_second = clock_second;
        const uint8_t glyph[6] = {h / 10, h % 10, m / 10,
                                  m % 10, s / 10, s % 10};
        for (int i = 0; i < 6; i++)
            set_target(&digits[i], glyph[i], now + (5 - i) * DIGIT_STAGGER_US);
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

    memset(buf, 0, WIDTH * HEIGHT * 3);
    draw_fireworks(buf, now);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int c = coverage[y][x];
            PIXEL_RED(buf, x, y) =
                (uint8_t)((c * r + (255 - c) * PIXEL_RED(buf, x, y)) / 255);
            PIXEL_GRN(buf, x, y) =
                (uint8_t)((c * g + (255 - c) * PIXEL_GRN(buf, x, y)) / 255);
            PIXEL_BLU(buf, x, y) =
                (uint8_t)((c * b + (255 - c) * PIXEL_BLU(buf, x, y)) / 255);
        }
    }

    return true;
}
