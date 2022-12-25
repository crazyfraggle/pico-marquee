import { set_pixel } from './pixels';

export const eol = (str: string, idx: number) => idx >= str.length;

export function stringBitmap(
  buf: Uint8Array,
  x: number,
  y: number,
  bitmap: (string | null)[],
  color: number,
  bg?: number
) {
  for (let l = 0; bitmap[l] !== null; l++) {
    const line = bitmap[l];
    for (let c = 0; line && !eol(line, c); c++) {
      if (line[c] == '#') {
        set_pixel(x + c, y + l, color);
      } else if (bg) {
        set_pixel(x + c, y + l, bg);
      }
    }
  }
}

export function renderNumRightAligned(
  buf: Uint8Array,
  right: number,
  y: number,
  score: number,
  color: number,
  bg?: number
) {
  let tmp = score;
  let oom = 1;
  do {
    const num = nums_5[tmp % 10];
    let startx = right - 5 * oom;
    stringBitmap(buf, startx, y, num, 0xffffff, 1);
    tmp = (tmp / 10) | 0;
    oom++;
  } while (tmp);
}

export const nums_5 = [
  [' ## ', '#  #', '#  #', '#  #', ' ## ', null],
  ['  ##', '   #', '   #', '   #', '   #', null],
  [' ## ', '#  #', '  # ', ' #  ', '####', null],
  [' ## ', '#  #', '  ##', '#  #', ' ## ', null],
  ['  ##', ' # #', '####', '   #', '   #', null],
  ['####', '#   ', '### ', '   #', '### ', null],
  [' ## ', '#   ', '### ', '#  #', ' ## ', null],
  ['####', '   #', '  # ', ' #  ', '#   ', null],
  [' ## ', '#  #', ' ## ', '#  #', ' ## ', null],
  [' ## ', '#  #', ' ###', '   #', ' ## ', null]
];
