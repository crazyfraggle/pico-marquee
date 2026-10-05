// Mirrors demos/scroller.c: C64-style sine scroller. Every pixel column is
// lifted by a sine wave that rolls along while the text moves right to left.
import type { Demo } from '$lib/demo';
import { asciiToC64, c64GlyphRow } from '$lib/c64text';
import { get_render_buffer, HEIGHT, set_pixel, WIDTH } from '$lib/pixels';

const name = 'Scroller';

// Wave height in pixels either side of the centre line.
const WAVE_AMPLITUDE = 9;
// Sine table steps per screen column; 256 steps is one full period.
const WAVE_STRETCH = 2;
// Sine table steps the wave rolls each frame.
const WAVE_SPEED = 3;

const text = 'THIS IS A CLASSIC SCROLL TEXT DEMO FROM C64! ';
const colour = 0xffffff;

const sine = Array.from({ length: 256 }, (_, i) =>
  Math.round(127 * Math.sin((i * 2 * Math.PI) / 256))
);
let scrollX = WIDTH;
let phase = 0;

const init = () => {
  scrollX = WIDTH;
  phase = 0;
};

const render = (): boolean => {
  const buf = get_render_buffer();
  buf.fill(0, 0, WIDTH * HEIGHT * 3);

  const textW = text.length * 8;
  const centre = (HEIGHT - 8) / 2;

  for (let px = 0; px < WIDTH; px++) {
    const tx = px - scrollX;
    if (tx < 0 || tx >= textW) continue;

    const glyph = asciiToC64(text[Math.trunc(tx / 8)]);
    const bit = 7 - (tx % 8);
    const s = sine[(px * WAVE_STRETCH + phase) & 0xff];
    const top = centre + Math.trunc((s * WAVE_AMPLITUDE) / 127);

    for (let line = 0; line < 8; line++) {
      const py = top + line;
      if (py >= 0 && py < HEIGHT && (c64GlyphRow(glyph, line) >> bit) & 1) {
        set_pixel(px, py, colour);
      }
    }
  }

  phase = (phase + WAVE_SPEED) & 0xff;
  if (--scrollX < -textW) scrollX = WIDTH;

  return true;
};

const keyboard = (): boolean => false;

export const Scroller: Demo = {
  name,
  init,
  render,
  keyboard
};
