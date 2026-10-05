// Mirrors demos/matrix.c: green digital rain with the clock drifting on top.
// The preview uses the browser's local time instead of SNTP.
import type { Demo } from '../demo';
import { render_text_c64 } from '$lib/c64text';
import { BLU, get_render_buffer, GRN, HEIGHT, RED, rnd, WIDTH } from '../pixels';

const name = 'Matrix clock';

// Drop positions are in 1/16 pixel steps.
const SUBPIXEL = 16;
// Trail brightness is multiplied by TRAIL_DECAY/256 every frame.
const TRAIL_DECAY = 218;
// Frames a column stays empty before the next drop.
const RESPAWN_MAX_WAIT = 60;

// The rain colour at full brightness; trails are scaled from it.
const RAIN_R = 24;
const RAIN_G = 255;
const RAIN_B = 64;

// Near-black, since render_text_c64 treats 0 as "use the default colour".
const OUTLINE = 0x000001;
const CLOCK_COLOR = 0xcccccc;

interface Drop {
  y: number; // head position in subpixels
  speed: number; // subpixels per frame
  wait: number; // frames until the drop starts falling
}

const glow = new Uint8Array(WIDTH * HEIGHT);
const drops: Drop[] = [];
let phaseX = 0;
let phaseY = 0;

const spawnDrop = (d: Drop, maxWait: number) => {
  d.y = -(rnd() % (8 * SUBPIXEL));
  d.speed = 4 + (rnd() % 13); // 0.25 to 1 pixel per frame
  d.wait = maxWait > 0 ? rnd() % maxWait : 0;
};

const init = () => {
  glow.fill(0);
  drops.length = 0;

  // Start some drops part way down so the panel is not empty at first.
  for (let x = 0; x < WIDTH; x++) {
    const d: Drop = { y: 0, speed: 0, wait: 0 };
    spawnDrop(d, RESPAWN_MAX_WAIT);
    if (rnd() % 3 === 0) {
      d.y = rnd() % (HEIGHT * SUBPIXEL);
      d.wait = 0;
    }
    drops.push(d);
  }
};

const updateRain = () => {
  for (let i = 0; i < glow.length; i++) glow[i] = (glow[i] * TRAIL_DECAY) >> 8;

  drops.forEach((d, x) => {
    if (d.wait > 0) {
      d.wait--;
      return;
    }

    // Light every row the head passed this frame, so the trail has no gaps
    // when a drop moves more than a pixel.
    const from = Math.trunc(d.y / SUBPIXEL);
    d.y += d.speed;
    const to = Math.trunc(d.y / SUBPIXEL);
    for (let y = from; y <= to; y++) {
      if (y >= 0 && y < HEIGHT) glow[y * WIDTH + x] = 255;
    }

    if (to >= HEIGHT) spawnDrop(d, RESPAWN_MAX_WAIT);
  });
};

const drawRain = (buf: Uint8Array) => {
  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const g = glow[y * WIDTH + x];
      buf[RED(x, y)] = (g * RAIN_R) >> 8;
      buf[GRN(x, y)] = (g * RAIN_G) >> 8;
      buf[BLU(x, y)] = (g * RAIN_B) >> 8;
    }
  }

  // Leading pixels glow nearly white, as in the film.
  drops.forEach((d, x) => {
    const y = Math.trunc(d.y / SUBPIXEL);
    if (d.wait === 0 && y >= 0 && y < HEIGHT) {
      buf[RED(x, y)] = 170;
      buf[GRN(x, y)] = 255;
      buf[BLU(x, y)] = 170;
    }
  });
};

const drawClock = (buf: Uint8Array) => {
  const pad = (n: number) => String(n).padStart(2, '0');
  const now = new Date();
  const s = `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;

  // Drift slowly along a Lissajous path centred a little below the middle
  // of the panel.
  const twoPi = 2 * Math.PI;
  phaseX = (phaseX + 0.00628) % twoPi;
  phaseY = (phaseY + 0.00931) % twoPi;

  const textW = 8 * 8;
  const x = (WIDTH - textW) / 2 + Math.round(24 * Math.sin(phaseX));
  const y = 14 + Math.round(4 * Math.sin(phaseY));

  // A dark outline keeps the digits readable over bright trails.
  for (let dy = -1; dy <= 1; dy++)
    for (let dx = -1; dx <= 1; dx++)
      if (dx !== 0 || dy !== 0) render_text_c64(buf, x + dx, y + dy, s, OUTLINE, -1);

  render_text_c64(buf, x, y, s, CLOCK_COLOR, -1);
};

const render = (): boolean => {
  const buf = get_render_buffer();

  updateRain();
  drawRain(buf);
  drawClock(buf);

  return true;
};

const keyboard = (): boolean => false;

export const Matrix: Demo = {
  name,
  init,
  render,
  keyboard
};
