#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Minimal lwIP configuration for the pico_cyw43_arch_lwip_poll variant.
// Based on the pico-examples common lwipopts; trimmed to what the TCP
// command listener in wifi.c needs (TCP server, DHCP client, no app protocols).

// We run lwIP without an RTOS (bare poll loop on core0).
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Memory: lwIP manages its own pools rather than the C heap.
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// Protocols.
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1

// DHCP: don't probe the address with ARP before using it (faster bring-up).
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// TCP tuning.
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

#define LWIP_CHKSUM_ALGORITHM       3

// Stats / debug off for a lean release build.
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

#define LWIP_DEBUG                  0
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF

#endif /* _LWIPOPTS_H */
