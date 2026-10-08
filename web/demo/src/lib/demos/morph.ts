// Mirrors demos/morph.c: a full-panel HH:MM:SS clock whose digits morph into
// each other, tinted with a slowly drifting colour. The preview uses the
// browser's local time instead of SNTP.
import type { Demo } from '../demo';
import { BLU, get_render_buffer, GRN, HEIGHT, RED, WIDTH } from '../pixels';

const name = 'Morph clock';

const FIX = 16;
const SEGMENTS = 4;
// Brush stamps per segment.
const STEPS = 32;
const MORPH_MS = 750;

// Brush radius in pixels, plus half a pixel of anti-aliasing either side.
const BRUSH_RADIUS = 1.5;
const BRUSH_REACH = 2;
const BRUSH_SIZE = 2 * BRUSH_REACH + 1;
// Brush positions are quantised to quarter pixels.
const BRUSH_SUB = 4;

// Colour channels swing between these levels, each with its own period.
const LEVEL_MIN = 51; // 20%
const LEVEL_MAX = 178; // 70%
const PERIOD_R_MS = 23000;
const PERIOD_G_MS = 37000;
const PERIOD_B_MS = 53000;

type Path = number[]; // x0, y0, x1, y1, ... in 1/16 pixel

const P = (...xy: number[]): Path => xy.map((v) => Math.trunc(v * FIX));

// Glyph paths in a 13 x 27 pixel box, measured on the stroke centre line.
// 10 is the dash shown before the clock is set.
const GLYPH_DASH = 10;
// prettier-ignore
const glyphs: Path[] = [
  P(6.5, 0, 10.09, 0, 13, 6.04, 13, 13.5, 13, 20.96, 10.09, 27, 6.5, 27,
    2.91, 27, 0, 20.96, 0, 13.5, 0, 6.04, 2.91, 0, 6.5, 0),
  P(2.5, 5, 4.5, 4, 6.5, 2, 7.5, 0, 7.5, 3, 7.5, 6, 7.5, 9,
    7.5, 12, 7.5, 15, 7.5, 18, 7.5, 21, 7.5, 24, 7.5, 27),
  P(0.5, 6.5, 0.5, 2.7, 3.3, 0, 6.5, 0, 10, 0, 12.5, 3, 12.5, 7,
    12.5, 12, 5, 17, 0, 27, 4.33, 27, 8.67, 27, 13, 27),
  P(0.5, 4, 2, 1.2, 4.2, 0, 6.5, 0, 14, 0, 12.5, 13, 5, 13,
    13.5, 13, 14, 27, 6.5, 27, 4, 27, 1.8, 25.8, 0.5, 23),
  P(13, 19, 8.67, 19, 4.33, 19, 0, 19, 3.17, 12.67, 6.33, 6.33, 9.5, 0,
    9.5, 4.5, 9.5, 9, 9.5, 13.5, 9.5, 18, 9.5, 22.5, 9.5, 27),
  P(12, 0, 8.5, 0, 5, 0, 1.5, 0, 1.17, 4, 0.83, 8, 0.5, 12,
    4, 9.5, 12.5, 9.5, 12.5, 19, 12.5, 28.5, 3, 28.5, 0.5, 24),
  P(11, 1.5, 8, -1, 0, 2, 0, 16, 0, 23, 3, 27, 6.5, 27,
    14, 27, 14, 11, 6.5, 11, 3, 11, 0.5, 13.5, 0.3, 17),
  P(0, 0, 2.17, 0, 4.33, 0, 6.5, 0, 8.67, 0, 10.83, 0, 13, 0,
    11.3, 4.3, 9.5, 8.5, 8, 13, 6.8, 16.8, 5.5, 21.5, 4.5, 27),
  P(6.5, 12.5, 0.5, 11, 0.5, 0, 6.5, 0, 12.5, 0, 12.5, 11, 6.5, 12.5,
    0, 14, 0, 27, 6.5, 27, 13, 27, 13, 14, 6.5, 12.5),
  P(2, 25.5, 5, 28, 13, 25, 13, 11, 13, 4, 10, 0, 6.5, 0,
    -1, 0, -1, 16, 6.5, 16, 10, 16, 12.5, 13.5, 12.7, 10),
  P(2, 13.5, 2.75, 13.5, 3.5, 13.5, 4.25, 13.5, 5, 13.5, 5.75, 13.5, 6.5, 13.5,
    7.25, 13.5, 8, 13.5, 8.75, 13.5, 9.5, 13.5, 10.25, 13.5, 11, 13.5)
];

// Top-left of each digit's glyph box, and the colon centres.
const digitPos = P(5, 2.5, 24, 2.5, 48, 2.5, 67, 2.5, 91, 2.5, 110, 2.5);
const colonX = P(42.5, 85.5);
const colonY = P(10.5, 21.5);

interface Morph {
  from: number;
  to: number;
  start: number; // ms
}

const digits: Morph[] = [];
const coverage = new Uint8Array(WIDTH * HEIGHT);
const brush: Uint8Array[] = [];

const buildBrush = () => {
  for (let sy = 0; sy < BRUSH_SUB; sy++)
    for (let sx = 0; sx < BRUSH_SUB; sx++) {
      const b = new Uint8Array(BRUSH_SIZE * BRUSH_SIZE);
      const cx = (sx + 0.5) / BRUSH_SUB;
      const cy = (sy + 0.5) / BRUSH_SUB;
      for (let dy = 0; dy < BRUSH_SIZE; dy++)
        for (let dx = 0; dx < BRUSH_SIZE; dx++) {
          const px = dx - BRUSH_REACH + 0.5 - cx;
          const py = dy - BRUSH_REACH + 0.5 - cy;
          const c = Math.min(1, Math.max(0, BRUSH_RADIUS + 0.5 - Math.hypot(px, py)));
          b[dy * BRUSH_SIZE + dx] = Math.trunc(c * 255 + 0.5);
        }
      brush.push(b);
    }
};

// Stamp the brush centred on (x, y) in 1/16 pixel, keeping the brighter
// coverage where it overlaps earlier stamps.
const stamp = (x: number, y: number) => {
  const ix = x >> 4;
  const iy = y >> 4;
  const sub = FIX / BRUSH_SUB;
  const b = brush[Math.trunc((y & 15) / sub) * BRUSH_SUB + Math.trunc((x & 15) / sub)];

  for (let dy = 0; dy < BRUSH_SIZE; dy++) {
    const py = iy + dy - BRUSH_REACH;
    if (py < 0 || py >= HEIGHT) continue;
    for (let dx = 0; dx < BRUSH_SIZE; dx++) {
      const px = ix + dx - BRUSH_REACH;
      if (px < 0 || px >= WIDTH) continue;
      const c = b[dy * BRUSH_SIZE + dx];
      if (c > coverage[py * WIDTH + px]) coverage[py * WIDTH + px] = c;
    }
  }
};

const drawPath = (p: Path, ox: number, oy: number) => {
  const n3 = STEPS * STEPS * STEPS;
  for (let s = 0; s < SEGMENTS; s++) {
    const o = s * 6;
    for (let i = s === 0 ? 0 : 1; i <= STEPS; i++) {
      const t = i;
      const u = STEPS - i;
      const a = u * u * u;
      const b = 3 * u * u * t;
      const c = 3 * u * t * t;
      const d = t * t * t;
      const x = Math.trunc((a * p[o] + b * p[o + 2] + c * p[o + 4] + d * p[o + 6]) / n3);
      const y = Math.trunc((a * p[o + 1] + b * p[o + 3] + c * p[o + 5] + d * p[o + 7]) / n3);
      stamp(ox + x, oy + y);
    }
  }
};

const setTarget = (m: Morph, glyph: number, now: number) => {
  if (glyph === m.to) return;
  m.from = m.to;
  m.to = glyph;
  m.start = now;
};

const drawDigit = (m: Morph, ox: number, oy: number, now: number) => {
  const elapsed = now - m.start;
  if (m.from === m.to || elapsed >= MORPH_MS) {
    drawPath(glyphs[m.to], ox, oy);
    return;
  }

  // Smoothstep ease-in-out, with t in 0..256.
  const t = Math.trunc((elapsed * 256) / MORPH_MS);
  const e = Math.trunc((t * t * (3 * 256 - 2 * t)) / (256 * 256));

  const a = glyphs[m.from];
  const b = glyphs[m.to];
  const path = a.map((v, i) => v + Math.trunc(((b[i] - v) * e) / 256));
  drawPath(path, ox, oy);
};

const channelLevel = (now: number, period: number) => {
  const phase = ((now % period) / period) * 2 * Math.PI;
  return Math.trunc(
    (LEVEL_MIN + LEVEL_MAX) / 2 + ((LEVEL_MAX - LEVEL_MIN) / 2) * Math.sin(phase) + 0.5
  );
};

const init = () => {
  if (brush.length === 0) buildBrush();

  // Start on dashes; the first frame morphs straight to the time.
  digits.length = 0;
  for (let i = 0; i < 6; i++) digits.push({ from: GLYPH_DASH, to: GLYPH_DASH, start: 0 });
};

const render = (): boolean => {
  const buf = get_render_buffer();
  const now = performance.now();
  const date = new Date();
  const h = date.getHours();
  const m = date.getMinutes();
  const s = date.getSeconds();
  const glyph = [
    Math.trunc(h / 10),
    h % 10,
    Math.trunc(m / 10),
    m % 10,
    Math.trunc(s / 10),
    s % 10
  ];
  digits.forEach((d, i) => setTarget(d, glyph[i], now));

  coverage.fill(0);
  digits.forEach((d, i) => drawDigit(d, digitPos[i * 2], digitPos[i * 2 + 1], now));
  for (const x of colonX) for (const y of colonY) stamp(x, y);

  const r = channelLevel(now, PERIOD_R_MS);
  const g = channelLevel(now, PERIOD_G_MS);
  const b = channelLevel(now, PERIOD_B_MS);

  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const c = coverage[y * WIDTH + x];
      buf[RED(x, y)] = Math.trunc((c * r) / 255);
      buf[GRN(x, y)] = Math.trunc((c * g) / 255);
      buf[BLU(x, y)] = Math.trunc((c * b) / 255);
    }
  }

  return true;
};

const keyboard = (): boolean => false;

export const Morph: Demo = {
  name,
  init,
  render,
  keyboard
};
