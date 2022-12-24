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

// Game state
let dir_x = 0;
let dir_y = 0;
enum GameState {
  INIT = 0,
  WAITING,
  MOVING,
  EATING,
  DYING,
  GAMEOVER
}

let game_state: GameState = GameState.INIT;
let score = 0; // == length
let high = 0;
// Make game area 30x100 pixels. Max length of Snek is 3000.
// static uint8_t snek_x[3000];
// static uint8_t snek_y[3000];
const snek_x = new Uint8Array(3000);
const snek_y = new Uint16Array(3000);

const init = () => {
  console.log('Snek - The game');
  clear_buffers();

  snek_x[0] = 50;
  snek_y[0] = 16;
  snek_x[1] = 49;
  snek_y[1] = 16;
  dir_x = dir_y = 0;
  score = 1;

  // Draw boundary
  for (let x = 0; x <= 101; x++) {
    set_pixel(x, 0, 0x820000);
    set_pixel(x, 31, 0x820000);
  }
  for (let y = 1; y <= 30; y++) {
    set_pixel(0, y, 0x820000);
    set_pixel(101, y, 0x820000);
  }

  // Draw score text
  for (let l = 0; score_text[l] != null; l++) {
    const line = score_text[l];
    for (let c = 0; line?.[c]; c++) {
      if (line[c] == '#') {
        set_pixel(102 + c, 0 + l, 0x334455);
      }
    }
  }
  for (let l = 0; high_text[l] != null; l++) {
    const line = high_text[l];
    for (let c = 0; line?.[c]; c++) {
      if (line[c] == '#') {
        set_pixel(102 + c, 16 + l, 0x554433);
      }
    }
  }

  game_state = GameState.INIT;

  flip_buffer(true);
};

const render = (): boolean => {
  const buf = get_render_buffer();
  // buf: Uint8Array = get_render_buffer();

  switch (game_state) {
    case GameState.INIT:
      state_init(buf);
      break;

    case GameState.WAITING:
      break;

    case GameState.MOVING:
      state_moving(buf);
      break;

    case GameState.EATING:
      state_eating(buf);
      break;

    case GameState.DYING:
      // Some end of game signal
      state_dying(buf);
      break;

    case GameState.GAMEOVER:
      // Some game over graphics.
      // echo_all("\r\nGame over. Press S to restart\r\n", 34);
      state_gameover(buf);
      break;

    default:
      break;
  }

  return true;
};

const keyboard = (char: string): boolean => {
  console.log(`Snek key: ${char}`);
  switch (char) {
    case 'w':
      dir_x = 0;
      dir_y = -1;
      if (game_state == GameState.WAITING) game_state = GameState.MOVING;
      return true;

    case 'a':
      dir_x = -1;
      dir_y = 0;
      if (game_state == GameState.WAITING) game_state = GameState.MOVING;
      return true;

    case 's':
      dir_x = 0;
      dir_y = 1;
      if (game_state == GameState.WAITING) game_state = GameState.MOVING;
      return true;

    case 'd':
      dir_x = 1;
      dir_y = 0;
      if (game_state == GameState.WAITING) game_state = GameState.MOVING;
      return true;

    case 'p':
      dir_x = dir_y = 0;
      game_state = GameState.WAITING;
      return true;

    case 'n':
      if (game_state == GameState.GAMEOVER) {
        console.log('asads');
        init();
      }

      return true;

    default:
      break;
  }
  return false;
};

export const Snek: Demo = {
  name: 'Snek',
  init,
  render,
  keyboard
};

//
// Utilities
//

function add_food(buf: Uint8Array) {
  // Add a block of fud
  let fs = (rnd() & 3) + 2;
  let fx = rnd() % (100 - fs);
  let fy = rnd() % (30 - fs);
  console.log('add_food', fs, fx, fy);
  for (let y = 0; y < fs; y++) {
    for (let x = 0; x < fs; x++) {
      buf[RED(1 + x + fx, 1 + y + fy)] = 0xf1; // fs * 0x20 + 1;
      buf[GRN(1 + x + fx, 1 + y + fy)] = 0x00; // fs * 0x10 + 1;
      buf[BLU(1 + x + fx, 1 + y + fy)] = 0x00; // fs * 0x30 + 1;
    }
  }
}

function increase_score(amount: number) {
  while (amount--) {
    score++;
    snek_x[score] = snek_x[score - 1];
    snek_y[score] = snek_y[score - 1];
  }
}

function render_highscore(buf: Uint8Array) {
  // Draw high score
  let tmp = high;
  let oom = 1;
  do {
    const num = nums[tmp % 10];
    let startx = 127 - 5 * oom;
    for (let l = 0; num[l] != null; l++) {
      for (let c = 0; c < 4 && num[l]?.[c]; c++) {
        set_pixel(startx + c, 25 + l, 0);
        if (num[l]?.[c] == '#') set_pixel(startx + c, 25 + l, 0xffffff);
      }
    }
    tmp = (tmp / 10) | 0;
    oom++;
  } while (tmp);
}

/////////////////////////////////
// Internal game state handlers
//
function state_init(buf: Uint8Array) {
  add_food(buf);
  render_highscore(buf);
  game_state = GameState.MOVING;
}

function state_moving(buf: Uint8Array) {
  // Advance Snek
  for (let i = score; i > 0; i--) {
    snek_x[i] = snek_x[i - 1];
    snek_y[i] = snek_y[i - 1];
  }
  // New head
  snek_x[0] += dir_x;
  snek_y[0] += dir_y;

  // Collision detect
  // Border hit?
  if (buf[BLU(snek_x[0], snek_y[0])] == 0x82) {
    // echo_all("Border hit\r\n", 13);
    game_state = GameState.DYING;
  }
  // Self hit?
  if (buf[RED(snek_x[0], snek_y[0])] == 0x82) {
    // echo_all("Hit self\r\n", 11);
    game_state = GameState.DYING;
  }
  // Food hit
  if ((buf[RED(snek_x[0], snek_y[0])] & 0x01) == 1) {
    // echo_all("Got fud\r\n", 10);
    increase_score(1);
    game_state = GameState.EATING;
  }

  // Draw snek
  set_pixel(snek_x[1], snek_y[1], 0x7f7f82);
  set_pixel(snek_x[score], snek_y[score], 0);
  set_pixel(snek_x[0], snek_y[0], 0xfffffe);
}

function state_eating(buf: Uint8Array) {
  // Reduce food block by 1 pixel
  // - Find food pixel
  // - Increase score by 1
  // - Clear food pixel
  // - Copy tail pixel to new score
  // Until no more food.
  let found = false;
  for (let y = 1; y < 31 && !found; y++) {
    for (let x = 1; x < 100 && !found; x++) {
      if ((buf[RED(x, y)] & 0x01) == 0x01) {
        increase_score(1);
        set_pixel(x, y, 0);

        // Draw score
        let tmp = score;
        let oom = 1;
        while (tmp) {
          const num = nums[tmp % 10];
          console.log(num);
          let startx = 127 - 5 * oom;
          for (let l = 0; num[l] != null; l++) {
            for (let c = 0; c < 4 && num[l]?.[c]; c++) {
              set_pixel(startx + c, 9 + l, 0);
              if (num[l]?.[c] == '#') set_pixel(startx + c, 9 + l, 0xffffff);
            }
          }
          tmp = (tmp / 10) | 0;
          oom++;
        }

        found = true;
      }
    }
  }

  // No more food pixels? Move along Snek!
  if (!found) {
    add_food(buf);
    game_state = GameState.MOVING;
  }
}
function state_dying(buf: Uint8Array) {
  if (score > high) {
    high = score;
  }
  render_highscore(buf);
  game_state = GameState.GAMEOVER;
}

function state_gameover(buf: Uint8Array) {}

// Text constants
const score_text = [
  ' ## ',
  '#     ##   ##  # #   ##',
  '#    #  # #  # ## # #  #',
  ' ##  #    #  # #    ###',
  '   # #    #  # #    #',
  '#  # #  # #  # #    #  #',
  ' ##   ##   ##  #     ##',
  null
];

const high_text = [
  '#  #        #',
  '#  # #      #',
  '#  #    ##  ###',
  '#### # #  # #  #',
  '#  # # #  # #  #',
  '#  # #  ### #  #',
  '#  # #    # #  #',
  '       ###',
  null
];

const nums = [
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
