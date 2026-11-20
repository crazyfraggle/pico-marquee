#ifndef HA_MQTT_H
#define HA_MQTT_H

// Home Assistant integration over MQTT with discovery. Exposes the panel as a
// light (power, brightness, mode as effect), a mode select and a scroller
// text entity. Compiled to no-ops unless the board has WiFi and the build
// sets MQTT_HOST.

// (Re)start connecting to the broker. Called by wifi.c once the network is up.
void ha_mqtt_start(void);

// Drive reconnects, the outgoing queue and state publishing. Called from
// wifi_task() after lwIP has been polled.
void ha_mqtt_task(void);

#endif /* HA_MQTT_H */
