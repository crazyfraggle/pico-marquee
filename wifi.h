#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

// Bring up the CYW43 chip, join the configured WiFi network, and start the
// TCP command listener. Returns true on success. On boards without WiFi
// (PICO_CYW43_SUPPORTED undefined) this is a no-op that returns false.
bool wifi_init(void);

// Pump the lwIP stack. Call once per main-loop iteration. No-op without WiFi.
void wifi_task(void);

// True once an IP address has been obtained and the listener is up.
bool wifi_is_connected(void);

#endif /* WIFI_H */
