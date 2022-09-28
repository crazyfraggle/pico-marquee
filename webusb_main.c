/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/* This example demonstrates WebUSB as web serial with browser with WebUSB support (e.g Chrome).
 * After enumerated successfully, browser will pop-up notification
 * with URL to landing page, click on it to test
 *  - Click "Connect" and select device, When connected the on-board LED will litted up.
 *  - Any charters received from either webusb/Serial will be echo back to webusb and Serial
 *
 * Note:
 * - The WebUSB landing page notification is currently disabled in Chrome
 * on Windows due to Chromium issue 656702 (https://crbug.com/656702). You have to
 * go to landing page (below) to test
 *
 * - On Windows 7 and prior: You need to use Zadig tool to manually bind the
 * WebUSB interface with the WinUSB driver for Chrome to access. From windows 8 and 10, this
 * is done automatically by firmware.
 *
 * - On Linux/macOS, udev permission may need to be updated by
 *   - copying '/examples/device/99-tinyusb.rules' file to /etc/udev/rules.d/ then
 *   - run 'sudo udevadm control --reload-rules && sudo udevadm trigger'
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "pixels.h"
#include "demos.h"

#define LED_PIN PICO_DEFAULT_LED_PIN

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum
{
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,

  BLINK_ALWAYS_ON = UINT32_MAX,
  BLINK_ALWAYS_OFF = 0
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

#define URL "localhost:5173"

const tusb_desc_webusb_url_t desc_url =
    {
        .bLength = 3 + sizeof(URL) - 1,
        .bDescriptorType = 3, // WEBUSB URL type
        .bScheme = 1,         // 0: http, 1: https
        .url = URL};

static bool web_serial_connected = false;

//------------- prototypes -------------//
void led_blinking_task(void);
void cdc_task(void);
void webserial_task(void);
void render_task(void);

/*------------- MAIN -------------*/
int webusb_main(void)
{
  board_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    cdc_task();
    webserial_task();
    led_blinking_task();
    render_task();
  }

  return 0;
}

// send characters to both CDC and WebUSB
void echo_all(uint8_t buf[], uint32_t count)
{
  // echo to web serial
  if (web_serial_connected)
  {
    tud_vendor_write(buf, count);
  }

  // echo to cdc
  if (tud_cdc_connected())
  {
    for (uint32_t i = 0; i < count; i++)
    {
      tud_cdc_write_char(buf[i]);

      if (buf[i] == '\r')
        tud_cdc_write_char('\n');
    }
    tud_cdc_write_flush();
  }
}

void handle_input_buffer(uint8_t buf[], uint32_t count)
{
  if (demo_keyboard_handler(buf[0]))
  {
    echo_all("Demokey\r\n", 9);

    return;
  }

  switch (buf[0])
  {
  case 0x42: // "B" - Jump to bootsel
    echo_all("Rebooting to BOOTSEL\r\n", 23);
    reset_usb_boot(0, 0);
    break;

  case 0x46: // "F" - Fire demo
    select_demo(1);
    echo_all("Enabling Fire Demo\r\n", 21);
    break;

  case 'S': // "S" - Snek game
    select_demo(2);
    echo_all("Enabling Snek Game\r\n", 21);
    break;

  case 0x50: // "P" - Pixel dump. Dumps a row of pixels.
    // Format Pxyn[rgb1,...,rgbn]
    {
      uint8_t x = buf[1];
      uint8_t y = buf[2];
      uint8_t n = buf[3]; // Number of pixels (3 bytes each)

      uint8_t *pbuf = get_render_buffer();
      uint8_t *ptarget = &PIXEL_RED(pbuf, x, y); // &pbuf[(x + y * WIDTH) * 3];
      memcpy(ptarget, &buf[4], n * 3);

      break;
    }

  default:
    echo_all(buf, count);
    break;
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP)
    return true;

  switch (request->bmRequestType_bit.type)
  {
  case TUSB_REQ_TYPE_VENDOR:
    switch (request->bRequest)
    {
    case VENDOR_REQUEST_WEBUSB:
      // match vendor request in BOS descriptor
      // Get landing page url
      return tud_control_xfer(rhport, request, (void *)&desc_url, desc_url.bLength);

    case VENDOR_REQUEST_MICROSOFT:
      if (request->wIndex == 7)
      {
        // Get Microsoft OS 2.0 compatible descriptor
        uint16_t total_len;
        memcpy(&total_len, desc_ms_os_20 + 8, 2);

        return tud_control_xfer(rhport, request, (void *)desc_ms_os_20, total_len);
      }
      else
      {
        return false;
      }

    default:
      break;
    }
    break;

  case TUSB_REQ_TYPE_CLASS:
    if (request->bRequest == 0x22)
    {
      // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
      web_serial_connected = (request->wValue != 0);

      // Always lit LED if connected
      if (web_serial_connected)
      {
        gpio_put(LED_PIN, 1);
        blink_interval_ms = BLINK_ALWAYS_ON;

        tud_vendor_write_str("\nTinyUSB WebUSB device example\n");
      }
      else
      {
        blink_interval_ms = BLINK_MOUNTED;
      }

      // response with status OK
      return tud_control_status(rhport, request);
    }
    break;

  default:
    break;
  }

  // stall unknown request
  return false;
}

void webserial_task(void)
{
  if (web_serial_connected)
  {
    if (tud_vendor_available())
    {
      uint8_t buf[64];
      uint32_t count = tud_vendor_read(buf, sizeof(buf));

      handle_input_buffer(buf, count);
    }
  }
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  if (tud_cdc_connected())
  {
    // connected and there are data available
    if (tud_cdc_available())
    {
      uint8_t buf[64];

      uint32_t count = tud_cdc_read(buf, sizeof(buf));

      handle_input_buffer(buf, count);
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void)itf;

  // connected
  if (dtr && rts)
  {
    // print initial message when connected
    tud_cdc_write_str("\r\nTinyUSB WebUSB device example\r\n");
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void)itf;
}

// uint32_t board_millis()
// {
//   return to_ms_since_boot(get_absolute_time());
// }

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = 0;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms)
    return; // not enough time
  start_ms += blink_interval_ms;

  // board_led_write(led_state);
  gpio_put(LED_PIN, led_state);

  led_state ^= 1; // toggle
}

//--------------------------------------------------------------------+
// RENDER TASK
//--------------------------------------------------------------------+
#define FPS 1000 / 50
void render(int frameCount)
{
  // Demo code is active and rendered.
  if (render_demo())
    return;
  // render_demo();
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
#define FRAME_TIME 40
void render_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = 0;

  // Blink every interval ms
  if (board_millis() - start_ms < FRAME_TIME)
    return; // not enough time
  start_ms += FRAME_TIME;

  flip_buffer(true);
  render(0);
}
