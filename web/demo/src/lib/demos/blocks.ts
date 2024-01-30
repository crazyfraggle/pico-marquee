import type { Demo } from '$lib/demo';
import {
  BLU,
  clear_buffers,
  flip_buffer,
  get_render_buffer,
  GRN,
  RED,
  rnd,
  rgb,
  set_pixel
} from '$lib/pixels';
import { nums_5, renderNumRightAligned, stringBitmap } from '$lib/text';
import { render_text_c64 } from '$lib/c64text';

const name = 'Blocks';

const init = () => {
  console.log('Blocks demo');
  clear_buffers();
  flip_buffer(false);
};

let x = 128;
let y = 0;
let yVel = 1;
let xVel = -1;

const render = (): boolean => {
  const buf = get_render_buffer();
  const x1 = rnd() % 128;
  const y1 = rnd() % 32;
  const x2 = rnd() % 128;
  const y2 = rnd() % 32;

  // Sort the coordinates
  const minX = Math.min(x1, x2);
  const maxX = Math.max(x1, x2);
  const minY = Math.min(y1, y2);
  const maxY = Math.max(y1, y2);

  // Draw rectangle between sorted coordinates
  const color = rgb(rnd() % 256, rnd() % 256, rnd() % 256);
  for (let i = minX; i < maxX; i++) {
    for (let j = minY; j < maxY; j++) {
      set_pixel(i, j, color);
    }
  }

  flip_buffer(true);
  return true;
};

const keyboard = (char: string): boolean => {
  return false;
};

export const Blocks: Demo = {
  name,
  init,
  render,
  keyboard
};
