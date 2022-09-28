// Snake game
//
// Good old snake. Food ranges from 2x2 to 5x5 pixels. Differentiate by color for easy detection of hit.
// Border around outer limit
// Score rendered on edge.
// Control with WASD over serial

#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#include "bsp/board.h"

#include "pixels.h"
#include "snek.h"

// USB utility
// void echo_all(uint8_t buf[], uint32_t count);

// State handlers
void state_init(uint8_t *);
void state_moving(uint8_t *);
void state_eating(uint8_t *);
void state_dying(uint8_t *);
void state_gameover(uint8_t *);

// Game state
static char dir_x = 0;
static char dir_y = 0;
static enum {
    INIT = 0,
    WAITING,
    MOVING,
    EATING,
    DYING,
    GAMEOVER
} game_state = 0;
static int score = 0; // == length
static int high = 0;
// Make game area 30x100 pixels. Max length of Snek is 3000.
static uint8_t snek_x[3000];
static uint8_t snek_y[3000];

//
// Exports
//
void init_snek()
{
    clear_buffers();

    snek_x[0] = 50;
    snek_y[0] = 16;
    snek_x[1] = 49;
    snek_y[1] = 16;
    dir_x = dir_y = 0;
    score = 1;

    // Draw boundary
    for (int x = 0; x <= 101; x++)
    {
        set_pixel(x, 0, 0x820000);
        set_pixel(x, 31, 0x820000);
    }
    for (int y = 1; y <= 30; y++)
    {
        set_pixel(0, y, 0x820000);
        set_pixel(101, y, 0x820000);
    }

    // Draw score text
    for (int l = 0; score_text[l] != NULL; l++)
    {
        char *line = score_text[l];
        for (int c = 0; line[c] != 0; c++)
        {
            if (line[c] == '#')
            {
                set_pixel(102 + c, 0 + l, 0x334455);
            }
        }
    }
    for (int l = 0; high_text[l] != NULL; l++)
    {
        char *line = high_text[l];
        for (int c = 0; line[c] != 0; c++)
        {
            if (line[c] == '#')
            {
                set_pixel(102 + c, 16 + l, 0x554433);
            }
        }
    }

    game_state = INIT;
}

bool render_snek()
{
    uint8_t *buf = get_render_buffer();

    switch (game_state)
    {
    case INIT:
        state_init(buf);
        break;

    case WAITING:
        break;

    case MOVING:
        state_moving(buf);
        break;

    case EATING:
        state_eating(buf);
        break;

    case DYING:
        // Some end of game signal
        state_dying(buf);
        break;

    case GAMEOVER:
        // Some game over graphics.
        // echo_all("\r\nGame over. Press S to restart\r\n", 34);
        state_gameover(buf);
        break;

    default:
        break;
    }

    return true;
}

bool snek_keyboard(char c)
{
    switch (c)
    {
    case 'w':
        dir_x = 0;
        dir_y = -1;
        if (game_state == WAITING)
            game_state = MOVING;
        return true;

    case 'a':
        dir_x = -1;
        dir_y = 0;
        if (game_state == WAITING)
            game_state = MOVING;
        return true;

    case 's':
        dir_x = 0;
        dir_y = 1;
        if (game_state == WAITING)
            game_state = MOVING;
        return true;

    case 'd':
        dir_x = 1;
        dir_y = 0;
        if (game_state == WAITING)
            game_state = MOVING;
        return true;

    case 'p':
        dir_x = dir_y = 0;
        game_state = WAITING;
        return true;

    case 'n':
        if (game_state == GAMEOVER)
            init_snek();
        return true;

    default:
        break;
    }
    return false;
}

//
// Utilities
//

void add_food(uint8_t *buf)
{
    // Add a block of fud
    int fs = (rnd & 3) + 2;
    int fx = rnd % (100 - fs);
    int fy = rnd % (30 - fs);
    for (int y = 0; y < fs; y++)
    {
        for (int x = 0; x < fs; x++)
        {
            PIXEL_RED(buf, 1 + x + fx, 1 + y + fy) = 0xf1; // fs * 0x20 + 1;
            PIXEL_GRN(buf, 1 + x + fx, 1 + y + fy) = 0x00; // fs * 0x10 + 1;
            PIXEL_BLU(buf, 1 + x + fx, 1 + y + fy) = 0x00; // fs * 0x30 + 1;
        }
    }
}

void increase_score(int amount)
{
    while (amount--)
    {
        score++;
        snek_x[score] = snek_x[score - 1];
        snek_y[score] = snek_y[score - 1];
    }
}

void render_highscore(uint8_t *buf)
{
    // Draw high score
    int tmp = high;
    int oom = 1;
    do
    {
        char **num = nums[tmp % 10];
        int startx = 127 - 5 * oom;
        for (int l = 0; num[l] != NULL; l++)
        {
            for (int c = 0; c < 4 && num[l][c] != 0; c++)
            {
                set_pixel(startx + c, 25 + l, 0);
                if (num[l][c] == '#')
                    set_pixel(startx + c, 25 + l, 0xffffff);
            }
        }
        tmp /= 10;
        oom++;
    } while (tmp);
}

/////////////////////////////////
// Internal game state handlers
//
void state_init(uint8_t *buf)
{
    add_food(buf);
    render_highscore(buf);
    game_state = MOVING;
}

void state_moving(uint8_t *buf)
{
    // Advance Snek
    for (int i = score; i > 0; i--)
    {
        snek_x[i] = snek_x[i - 1];
        snek_y[i] = snek_y[i - 1];
    }
    // New head
    snek_x[0] += dir_x;
    snek_y[0] += dir_y;

    // Collision detect
    // Border hit?
    if (PIXEL_BLU(buf, snek_x[0], snek_y[0]) == 0x82)
    {
        // echo_all("Border hit\r\n", 13);
        game_state = DYING;
    }
    // Self hit?
    if (PIXEL_RED(buf, snek_x[0], snek_y[0]) == 0x82)
    {
        // echo_all("Hit self\r\n", 11);
        game_state = DYING;
    }
    // Food hit
    if (PIXEL_RED(buf, snek_x[0], snek_y[0]) & 0x01 == 1)
    {
        // echo_all("Got fud\r\n", 10);
        increase_score(1);
        game_state = EATING;
    }

    // Draw snek
    set_pixel(snek_x[1], snek_y[1], 0x7f7f82);
    set_pixel(snek_x[score], snek_y[score], 0);
    set_pixel(snek_x[0], snek_y[0], 0xfffffe);
}

void state_eating(uint8_t *buf)
{
    // Reduce food block by 1 pixel
    // - Find food pixel
    // - Increase score by 1
    // - Clear food pixel
    // - Copy tail pixel to new score
    // Until no more food.
    bool found = false;
    for (int y = 1; y < 31 && !found; y++)
    {
        for (int x = 1; x < 100 && !found; x++)
        {
            if (PIXEL_RED(buf, x, y) & 0x01 == 0x01)
            {
                increase_score(1);
                set_pixel(x, y, 0);

                // Draw score
                int tmp = score;
                int oom = 1;
                while (tmp)
                {
                    char **num = nums[tmp % 10];
                    int startx = 127 - 5 * oom;
                    for (int l = 0; num[l] != NULL; l++)
                    {
                        for (int c = 0; c < 4 && num[l][c] != 0; c++)
                        {
                            set_pixel(startx + c, 9 + l, 0);
                            if (num[l][c] == '#')
                                set_pixel(startx + c, 9 + l, 0xffffff);
                        }
                    }
                    tmp /= 10;
                    oom++;
                }

                found = true;
            }
        }
    }

    // No more food pixels? Move along Snek!
    if (!found)
    {
        add_food(buf);
        game_state = MOVING;
    }
}
void state_dying(uint8_t *buf)
{
    if (score > high)
    {
        high = score;
    }
    render_highscore(buf);
    game_state = GAMEOVER;
}

void state_gameover(uint8_t *buf)
{
}
