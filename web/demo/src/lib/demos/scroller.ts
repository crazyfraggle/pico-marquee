import type { Demo } from '$lib/demo';
import {
    BLU,
    clear_buffers,
    flip_buffer,
    get_render_buffer,
    GRN,
    RED,
    rnd,
    set_pixel
} from '$lib/pixels';
import { nums_5, renderNumRightAligned, stringBitmap } from '$lib/text';
import { render_text_c64 } from '$lib/c64text';

const name = 'Scroller';

const init = () => {
    console.log('Scroller demo');
    clear_buffers();
    flip_buffer(false);
};

const text = "THIS IS A CLASSIC SCROLL TEXT DEMO FROM C64! ";

let x = 128;
let y = 0;
let yVel = 1;
let xVel = -1;

const render = (): boolean => {
    const buf = get_render_buffer();

    render_text_c64(buf, x, y, text, 0xffffff, 0x000001);
    x+=xVel;
    y+=yVel;
    if (y > 24) yVel = -1;
    if (y <= 0) yVel = 1;
    if (x < -text.length*8) x = 128;
    flip_buffer(false);
    return true;
  };
  

const keyboard = (char: string): boolean => {
    return false;
};


export const Scroller: Demo = {
    name,
    init,
    render,
    keyboard
};
