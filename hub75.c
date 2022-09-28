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

#include "tusb.h"
#include "pixels.h"
#include "demos.h"

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

void core1_main()
{
    PIO pio = pio0;
    uint sm_data = 0;
    uint sm_row = 1;

    // // Use the onboard LED as heartbeat indicator (and frame rate indicator)
    // gpio_init(LED_PIN);
    // gpio_set_dir(LED_PIN, GPIO_OUT);

    uint data_prog_offs = pio_add_program(pio, &hub75_data_rgb888_program);
    uint row_prog_offs = pio_add_program(pio, &hub75_row_program);
    hub75_data_rgb888_program_init(pio, sm_data, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75_row_program_init(pio, sm_row, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, STROBE_PIN);

    static uint32_t gc_row[2][WIDTH];

    while (1)
    {
        for (int rowsel = 0; rowsel < (1 << ROWSEL_N_PINS); ++rowsel)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                // Add two more rows here if doing parallell rendering.
                uint8_t *buf = get_display_buffer();
                int y1 = rowsel;
                int y2 = ((1u << ROWSEL_N_PINS) + rowsel);
                gc_row[0][x] = rgb(PIXEL_RED(buf, x, y1), PIXEL_GRN(buf, x, y1), PIXEL_BLU(buf, x, y1));
                gc_row[1][x] = rgb(PIXEL_RED(buf, x, y2), PIXEL_GRN(buf, x, y2), PIXEL_BLU(buf, x, y2));
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
