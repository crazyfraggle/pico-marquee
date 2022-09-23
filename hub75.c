/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "hub75.pio.h"
#include "usb_driver.h"

#include "tusb.h"
#include "pixels.h"

#define DATA_BASE_PIN 0
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 6
#define ROWSEL_N_PINS 4
#define CLK_PIN 11
#define STROBE_PIN 12
#define OEN_PIN 13

#define WIDTH 128
#define HEIGHT 32
#define ANIMTIMER 6

#define LED_PIN PICO_DEFAULT_LED_PIN

void render(int frameCount)
{
    static int x = 0;
    static int y = 0;
    static int xvel = 1;
    static int yvel = 2;

    x += xvel;
    if (x >= WIDTH)
    {
        xvel *= -1;
        x += xvel;
    }
    else if (x < 0)
    {
        xvel *= -1;
        x += xvel;
    }
    y += yvel;
    if (y >= HEIGHT)
    {
        yvel *= -1;
        y += yvel;
    }
    else if (y < 0)
    {
        yvel *= -1;
        y += yvel;
    }

    set_pixel(x, y, rgb(x * 2, y * 4, 44));
}

void core1_main()
{
    PIO pio = pio0;
    uint sm_data = 0;
    uint sm_row = 1;
    uint led_state = 0;

    // // Use the onboard LED as heartbeat indicator (and frame rate indicator)
    // gpio_init(LED_PIN);
    // gpio_set_dir(LED_PIN, GPIO_OUT);

    uint data_prog_offs = pio_add_program(pio, &hub75_data_rgb888_program);
    uint row_prog_offs = pio_add_program(pio, &hub75_row_program);
    hub75_data_rgb888_program_init(pio, sm_data, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75_row_program_init(pio, sm_row, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, STROBE_PIN);

    static uint32_t gc_row[2][WIDTH];
    int frameCount = 0;
    int scrollOffset = 0;
    uint scrollDelay = ANIMTIMER;
    int scrollDir = 1;

    while (1)
    {
        if (--scrollDelay == 0)
        {
            scrollDelay = ANIMTIMER;

            render(frameCount++);
        }
        for (int rowsel = 0; rowsel < (1 << ROWSEL_N_PINS); ++rowsel)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                // Add two more rows here if doing parallell rendering.
                int row0p = ((rowsel * WIDTH + x) * 3);
                int row1p = ((((1u << ROWSEL_N_PINS) + rowsel) * WIDTH + x) * 3);
                uint8_t *buf = get_buffer();
                gc_row[0][x] = rgb(buf[row0p], buf[row0p + 1], buf[row0p + 2]);
                gc_row[1][x] = rgb(buf[row1p], buf[row1p + 1], buf[row1p + 2]);
            }
            for (int bit = 0; bit < 8; ++bit)
            {
                hub75_data_rgb888_set_shift(pio, sm_data, data_prog_offs, bit);
                for (int x = 0; x < WIDTH; ++x)
                {
                    // Two more rows if parallell
                    pio_sm_put_blocking(pio, sm_data, gc_row[0][x]);
                    pio_sm_put_blocking(pio, sm_data, gc_row[1][x]);
                }
                // Dummy pixel per lane
                pio_sm_put_blocking(pio, sm_data, 0);
                pio_sm_put_blocking(pio, sm_data, 0);
                // SM is finished when it stalls on empty TX FIFO
                hub75_wait_tx_stall(pio, sm_data);
                // Also check that previous OEn pulse is finished, else things can get out of sequence
                hub75_wait_tx_stall(pio, sm_row);

                // Latch row data, pulse output enable for new row.
                pio_sm_put_blocking(pio, sm_row, rowsel | (100u * (1u << bit) << 5));
            }
        }
    }
}

// Forward declare. Yeah, this is cheating.
int webusb_main(void);

int main()
{
    // stdio_init_all();
    // setup_usb_device(buf1);

    multicore_launch_core1(core1_main);

    webusb_main();

    // Core 0 will be USB listener only
    while (1)
        tight_loop_contents();
}
