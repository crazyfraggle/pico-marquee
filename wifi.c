/*
 * WiFi transport for the LED marquee.
 *
 * On a WiFi-capable Pico (Pico W / Pico 2 W) this joins the configured network
 * and exposes the same single-byte command protocol used over USB on a TCP
 * port. Incoming bytes are handed to handle_input_buffer() -- the exact same
 * dispatcher cdc_task()/webserial_task() use -- so a network client can drive
 * the display (push pixels, switch demos, reboot, etc.) identically to USB.
 *
 * On a plain Pico (no CYW43) every function compiles to a no-op so the same
 * source tree still builds for USB-only hardware.
 */

#include "wifi.h"

#ifdef PICO_CYW43_SUPPORTED

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"

// Defined on the cmake command line (see CMakeLists.txt).
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MARQUEE_TCP_PORT
#define MARQUEE_TCP_PORT 4242
#endif

// The command dispatcher lives in webusb_main.c. The protocol's largest packet
// is the "P" pixel push (4-byte header + up to 16 RGB pixels). We accept up to
// a full panel row of pixels here to leave room for fatter frame commands that
// the wider TCP MTU makes practical.
extern void handle_input_buffer(uint8_t buf[], uint32_t count);

#define RX_BUF_SIZE 1024

static struct tcp_pcb *server_pcb = NULL;
static bool connected = false;

// --- per-connection receive ------------------------------------------------

static err_t on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  (void)arg;

  // NULL pbuf means the remote side closed the connection.
  if (p == NULL)
  {
    tcp_close(tpcb);
    return ERR_OK;
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    return err;
  }

  // Flatten the (possibly chained) pbuf into a contiguous buffer and dispatch.
  // Clients are expected to send one command per write; commands larger than
  // RX_BUF_SIZE are truncated, which only matters for oversized frame pushes.
  static uint8_t rxbuf[RX_BUF_SIZE];
  uint16_t len = p->tot_len > RX_BUF_SIZE ? RX_BUF_SIZE : p->tot_len;
  pbuf_copy_partial(p, rxbuf, len, 0);

  if (len > 0)
  {
    handle_input_buffer(rxbuf, len);
  }

  // Tell lwIP we've consumed the full pbuf and release it.
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  (void)arg;
  if (err != ERR_OK || newpcb == NULL)
  {
    return ERR_VAL;
  }

  tcp_recv(newpcb, on_recv);
  return ERR_OK;
}

static bool start_server(void)
{
  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (pcb == NULL)
  {
    return false;
  }

  if (tcp_bind(pcb, IP_ANY_TYPE, MARQUEE_TCP_PORT) != ERR_OK)
  {
    tcp_close(pcb);
    return false;
  }

  server_pcb = tcp_listen_with_backlog(pcb, 1);
  if (server_pcb == NULL)
  {
    tcp_close(pcb);
    return false;
  }

  tcp_accept(server_pcb, on_accept);
  return true;
}

// --- public API -------------------------------------------------------------

bool wifi_init(void)
{
  if (cyw43_arch_init() != 0)
  {
    return false;
  }

  cyw43_arch_enable_sta_mode();

  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                         CYW43_AUTH_WPA2_AES_PSK, 30000) != 0)
  {
    return false;
  }

  if (!start_server())
  {
    return false;
  }

  connected = true;
  return true;
}

void wifi_task(void)
{
  // Drives lwIP and the CYW43 driver. With the lwip_poll arch variant all
  // network callbacks (including on_recv above) run synchronously from here,
  // so they share the cooperative single-core-0 model the USB tasks use.
  cyw43_arch_poll();
}

bool wifi_is_connected(void)
{
  return connected;
}

#else /* !PICO_CYW43_SUPPORTED -- plain Pico, no WiFi hardware */

bool wifi_init(void) { return false; }
void wifi_task(void) {}
bool wifi_is_connected(void) { return false; }

#endif
