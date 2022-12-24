import type { Demo } from '../demo';
import {
  BLU,
  clear_buffers,
  flip_buffer,
  get_render_buffer,
  GRN,
  HEIGHT,
  RED,
  rnd,
  WIDTH
} from '../pixels';

const name = 'Fire';

const init = () => {
  console.log('Fire demo');
  clear_buffers();
  flip_buffer(false);
};

const render = (): boolean => {
  const buf = get_render_buffer();

  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 1; x < WIDTH - 1; x++) {
      buf[RED(x, y)] =
        (buf[RED(x, y)] + buf[RED(x - 1, y + 1)] + buf[RED(x, y + 1)] + buf[RED(x + 1, y + 1)]) >>
        2;
      buf[GRN(x, y)] = (buf[GRN(x - 1, y + 1)] + buf[GRN(x, y + 1)] + buf[GRN(x + 1, y + 1)]) >> 2;
      buf[BLU(x, y)] = (buf[BLU(x - 1, y + 1)] + buf[BLU(x, y + 1)] + buf[BLU(x + 1, y + 1)]) >> 2;
    }
  }

  // Clear bottom row
  for (let x = 0; x < WIDTH; x++) {
    buf[RED(x, HEIGHT)] = buf[RED(x, HEIGHT)] >> 1;
    buf[GRN(x, HEIGHT)] = buf[GRN(x, HEIGHT)] >> 1;
    buf[BLU(x, HEIGHT)] = buf[BLU(x, HEIGHT)] >> 1;
  }

  // Add pixels to hidden row.
  let newPixs = rnd() & 0x15;
  for (let i = 0; i < newPixs; i++) {
    let x = rnd() & 127;
    buf[RED(x, HEIGHT)] = 128 + (rnd() & 0x7f);
    buf[GRN(x, HEIGHT)] = rnd() & 0x7f;
    buf[BLU(x, HEIGHT)] = rnd() & 0x7f;
  }

  // render_text();
  flip_buffer(true);
  return true;
};

const keyboard = (char: string): boolean => {
  return false;
};

export const Fire: Demo = {
  name,
  init,
  render,
  keyboard
};
