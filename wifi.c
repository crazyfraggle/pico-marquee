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

#include <stdio.h>

#ifdef PICO_CYW43_SUPPORTED

#include <stdint.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/error.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
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
static bool cyw43_initialized = false;
static bool reconnect_requested = false;
static int last_connect_result = PICO_ERROR_INVALID_STATE;

static const char *connect_result_name(int result)
{
  switch (result)
  {
  case PICO_OK:
    return "connected";
  case PICO_ERROR_TIMEOUT:
    return "timeout/network not found";
  case PICO_ERROR_BADAUTH:
    return "authentication failed";
  case PICO_ERROR_CONNECT_FAILED:
    return "connection failed";
  case PICO_ERROR_INVALID_ARG:
    return "SSID is empty";
  case PICO_ERROR_INVALID_STATE:
    return "not attempted";
  default:
    return "driver error";
  }
}

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
  if (server_pcb != NULL)
  {
    return true;
  }

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

static bool connect_wifi(void)
{
  connected = false;

  if (WIFI_SSID[0] == '\0')
  {
    last_connect_result = PICO_ERROR_INVALID_ARG;
    printf("[wifi] Cannot connect: SSID is empty\r\n");
    return false;
  }

  if (!cyw43_initialized)
  {
    int init_result = cyw43_arch_init();
    if (init_result != PICO_OK)
    {
      last_connect_result = init_result;
      printf("[wifi] CYW43 initialization failed: %d\r\n", init_result);
      return false;
    }
    cyw43_initialized = true;
  }

  cyw43_arch_enable_sta_mode();
  cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);

  printf("[wifi] Connecting to \"%s\" (WPA2, 30 second timeout)...\r\n",
         WIFI_SSID);
  last_connect_result =
      cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                         CYW43_AUTH_WPA2_AES_PSK, 30000);
  if (last_connect_result != PICO_OK)
  {
    printf("[wifi] Connection failed: %s (%d)\r\n",
           connect_result_name(last_connect_result), last_connect_result);
    return false;
  }

  if (!start_server())
  {
    last_connect_result = PICO_ERROR_CONNECT_FAILED;
    printf("[wifi] Connected, but TCP listener on port %d failed\r\n",
           MARQUEE_TCP_PORT);
    return false;
  }

  connected = true;
  char status[96];
  wifi_format_status(status, sizeof(status));
  printf("[wifi] %s", status);
  return true;
}

bool wifi_init(void)
{
  return connect_wifi();
}

void wifi_task(void)
{
  if (!cyw43_initialized)
  {
    if (reconnect_requested)
    {
      reconnect_requested = false;
      connect_wifi();
    }
    return;
  }

  // Drives lwIP and the CYW43 driver. With the lwip_poll arch variant all
  // network callbacks (including on_recv above) run synchronously from here,
  // so they share the cooperative single-core-0 model the USB tasks use.
  cyw43_arch_poll();

  connected =
      cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;

  if (reconnect_requested)
  {
    reconnect_requested = false;
    connect_wifi();
  }
}

void wifi_request_reconnect(void)
{
  reconnect_requested = true;
}

void wifi_format_status(char *buf, size_t size)
{
  if (size == 0)
  {
    return;
  }

  if (!cyw43_initialized)
  {
    snprintf(buf, size, "WiFi: %s (%d)\r\n",
             connect_result_name(last_connect_result), last_connect_result);
    return;
  }

  int link_status =
      cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
  if (link_status == CYW43_LINK_UP && netif_default != NULL)
  {
    char ip[IP4ADDR_STRLEN_MAX];
    if (ip4addr_ntoa_r(netif_ip4_addr(netif_default), ip, sizeof(ip)) != NULL)
    {
      snprintf(buf, size, "WiFi: connected, IP %s, TCP port %d\r\n",
               ip, MARQUEE_TCP_PORT);
      return;
    }
  }

  snprintf(buf, size, "WiFi: offline; last attempt: %s (%d), link status %d\r\n",
           connect_result_name(last_connect_result), last_connect_result,
           link_status);
}

bool wifi_is_connected(void)
{
  return connected;
}

#else /* !PICO_CYW43_SUPPORTED -- plain Pico, no WiFi hardware */

bool wifi_init(void) { return false; }
void wifi_task(void) {}
void wifi_request_reconnect(void) {}
void wifi_format_status(char *buf, size_t size)
{
  if (size > 0)
  {
    snprintf(buf, size, "WiFi: unavailable on this board\r\n");
  }
}
bool wifi_is_connected(void) { return false; }

#endif
