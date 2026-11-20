/*
 * Home Assistant integration over MQTT.
 *
 * Connects to the broker named by MQTT_HOST with lwIP's MQTT client and
 * announces the panel through Home Assistant MQTT discovery:
 *
 *   light   power ON/OFF, brightness 0-255, mode as the light's effect
 *   select  mode (the same topic as the light's effect)
 *   text    scroller text; setting it switches to the scroller
 *
 * All topics live under pico-marquee/<board id>/. Commands arrive on
 * <topic>/set, state goes out retained on <topic>/state, and status carries
 * online/offline (the latter as the last will).
 *
 * State is published by polling: ha_mqtt_task() compares the demo, power,
 * brightness and scroller text with what it last sent. Changes made over
 * USB, the UART or TCP therefore reach Home Assistant too.
 *
 * Everything runs from the main loop. With the lwip_poll arch variant the
 * lwIP callbacks below are called from cyw43_arch_poll() in wifi_task(), on
 * core0, like the rest of the command handling.
 */

#include "ha_mqtt.h"

#if defined(PICO_CYW43_SUPPORTED) && defined(MARQUEE_MQTT)

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/time.h"
#include "pico/unique_id.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

#include "demos.h"
#include "pixels.h"
#include "demos/scroller.h"
#include "wifi.h"

// Defined on the cmake command line (see CMakeLists.txt).
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

#define DISCOVERY_PREFIX "homeassistant"
#define KEEP_ALIVE_S 60
#define RETRY_MIN_MS 5000
#define RETRY_MAX_MS 60000

// Outgoing work, sent in bit order. A send that does not fit in the client's
// output buffer or request list is retried on the next tick.
enum
{
    Q_SUB_CMD = 1u << 0,
    Q_SUB_HA = 1u << 1,
    Q_ONLINE = 1u << 2,
    Q_DISC_LIGHT = 1u << 3,
    Q_DISC_MODE = 1u << 4,
    Q_DISC_TEXT = 1u << 5,
    Q_DISCOVERY = Q_DISC_LIGHT | Q_DISC_MODE | Q_DISC_TEXT,
    Q_ALL = Q_SUB_CMD | Q_SUB_HA | Q_ONLINE | Q_DISCOVERY,
};

typedef enum
{
    MQ_IDLE,       // waiting for retry_at
    MQ_RESOLVING,  // DNS lookup in flight
    MQ_CONNECTING, // TCP + CONNECT in flight
    MQ_CONNECTED,
} mq_state_t;

typedef enum
{
    IN_NONE,
    IN_MODE,
    IN_POWER,
    IN_BRIGHTNESS,
    IN_TEXT,
    IN_HA_STATUS,
} in_topic_t;

static mqtt_client_t *client;
static mq_state_t state = MQ_IDLE;
static absolute_time_t retry_at;
static uint32_t retry_ms = RETRY_MIN_MS;
static uint32_t queue;

// Board id in lowercase hex, and the topic base built from it.
static char board_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
static char base[64];
static char client_id[64];
static char will_topic[80];

// What was last published, so only changes are sent.
static int pub_demo;
static int pub_on;
static int pub_brightness;
static bool pub_text_valid;
static char pub_text[SCROLLER_MAX_LEN + 1];

// Incoming publish being reassembled.
static in_topic_t in_topic;
static char in_buf[SCROLLER_MAX_LEN + 1];
static size_t in_len;
static bool in_overflow;

static char topic_buf[128];
static char payload_buf[768];

static void schedule_retry(void)
{
    state = MQ_IDLE;
    retry_at = make_timeout_time_ms(retry_ms);
    retry_ms = retry_ms * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : retry_ms * 2;
}

static void invalidate_published_state(void)
{
    pub_demo = -1;
    pub_on = -1;
    pub_brightness = -1;
    pub_text_valid = false;
}

// --- incoming ----------------------------------------------------------------

static void handle_command(in_topic_t topic, const char *payload)
{
    switch (topic)
    {
    case IN_MODE:
    {
        int num = demo_from_name(payload);
        if (num < 0)
        {
            printf("[mqtt] Unknown mode \"%s\"\r\n", payload);
            return;
        }
        display_on = true;
        select_demo(num);
        break;
    }

    case IN_POWER:
        if (strcmp(payload, "ON") == 0)
            display_on = true;
        else if (strcmp(payload, "OFF") == 0)
            display_on = false;
        break;

    case IN_BRIGHTNESS:
    {
        char *end;
        unsigned long level = strtoul(payload, &end, 10);
        if (end == payload || *end != '\0' || level > 255)
            return;
        display_brightness = (uint8_t)level;
        display_on = true;
        break;
    }

    case IN_TEXT:
        if (scroller_set_text(payload))
        {
            display_on = true;
            select_demo(DEMO_SCROLLER);
        }
        break;

    case IN_HA_STATUS:
        // Home Assistant restarted: announce the entities again. The state
        // topics are retained, but republish them as well to be safe.
        if (strcmp(payload, "online") == 0)
        {
            queue |= Q_DISCOVERY;
            invalidate_published_state();
        }
        break;

    default:
        break;
    }
}

static void on_incoming_publish(void *arg, const char *topic, u32_t tot_len)
{
    (void)arg;
    (void)tot_len;

    in_len = 0;
    in_overflow = false;
    in_topic = IN_NONE;

    if (strcmp(topic, DISCOVERY_PREFIX "/status") == 0)
    {
        in_topic = IN_HA_STATUS;
        return;
    }

    size_t base_len = strlen(base);
    if (strncmp(topic, base, base_len) != 0)
        return;

    const char *rest = topic + base_len;
    if (strcmp(rest, "/mode/set") == 0)
        in_topic = IN_MODE;
    else if (strcmp(rest, "/power/set") == 0)
        in_topic = IN_POWER;
    else if (strcmp(rest, "/brightness/set") == 0)
        in_topic = IN_BRIGHTNESS;
    else if (strcmp(rest, "/text/set") == 0)
        in_topic = IN_TEXT;
}

static void on_incoming_data(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    (void)arg;

    if (in_topic == IN_NONE)
        return;

    if (in_len + len > sizeof(in_buf) - 1)
        in_overflow = true;
    else
    {
        memcpy(&in_buf[in_len], data, len);
        in_len += len;
    }

    if (flags & MQTT_DATA_FLAG_LAST)
    {
        in_buf[in_len] = '\0';
        if (!in_overflow)
            handle_command(in_topic, in_buf);
        in_topic = IN_NONE;
    }
}

// --- connection --------------------------------------------------------------

static void on_connection(mqtt_client_t *c, void *arg, mqtt_connection_status_t status)
{
    (void)c;
    (void)arg;

    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("[mqtt] Connected to %s:%d as %s\r\n", MQTT_HOST, MQTT_PORT, base);
        state = MQ_CONNECTED;
        retry_ms = RETRY_MIN_MS;
        queue = Q_ALL;
        invalidate_published_state();
        return;
    }

    printf("[mqtt] Disconnected (%d), retrying in %lu ms\r\n", status,
           (unsigned long)retry_ms);
    schedule_retry();
}

static void connect_to(const ip_addr_t *addr)
{
    // lwIP copies these into the CONNECT packet straight away, so the struct
    // does not need to outlive the call.
    const struct mqtt_connect_client_info_t info = {
        .client_id = client_id,
        .client_user = MQTT_USER[0] ? MQTT_USER : NULL,
        .client_pass = MQTT_PASSWORD[0] ? MQTT_PASSWORD : NULL,
        .keep_alive = KEEP_ALIVE_S,
        .will_topic = will_topic,
        .will_msg = "offline",
        .will_qos = 1,
        .will_retain = 1,
    };

    err_t err = mqtt_client_connect(client, addr, MQTT_PORT, on_connection, NULL, &info);
    if (err != ERR_OK)
    {
        printf("[mqtt] Connect failed: %d\r\n", err);
        schedule_retry();
        return;
    }

    // mqtt_client_connect() clears the client, so this has to come after it.
    mqtt_set_inpub_callback(client, on_incoming_publish, on_incoming_data, NULL);
    state = MQ_CONNECTING;
}

static void on_dns_found(const char *name, const ip_addr_t *addr, void *arg)
{
    (void)arg;

    if (state != MQ_RESOLVING)
        return;

    if (addr == NULL)
    {
        printf("[mqtt] Cannot resolve %s\r\n", name);
        schedule_retry();
        return;
    }
    connect_to(addr);
}

static void start_connect(void)
{
    if (client == NULL)
    {
        client = mqtt_client_new();
        if (client == NULL)
        {
            printf("[mqtt] Out of memory for the client\r\n");
            schedule_retry();
            return;
        }
    }

    ip_addr_t addr;
    state = MQ_RESOLVING;
    err_t err = dns_gethostbyname(MQTT_HOST, &addr, on_dns_found, NULL);
    if (err == ERR_OK)
        connect_to(&addr);
    else if (err != ERR_INPROGRESS)
    {
        printf("[mqtt] DNS lookup failed: %d\r\n", err);
        schedule_retry();
    }
}

// --- outgoing ----------------------------------------------------------------

static err_t publish(const char *topic, const char *payload, bool retain)
{
    return mqtt_publish(client, topic, payload, (u16_t)strlen(payload), 0,
                        retain ? 1 : 0, NULL, NULL);
}

static err_t publish_sub(const char *sub, const char *payload)
{
    snprintf(topic_buf, sizeof(topic_buf), "%s/%s", base, sub);
    return publish(topic_buf, payload, true);
}

// Writes the JSON list of mode names, e.g. "dot","fire",... into out.
static void mode_list(char *out, size_t size)
{
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < DEMO_COUNT && used < size; i++)
    {
        int n = snprintf(&out[used], size - used, "%s\"%s\"", i ? "," : "", demo_name(i));
        if (n < 0)
            break;
        used += (size_t)n;
    }
}

// Builds topic_buf and payload_buf for one discovery message.
static bool build_discovery(uint32_t item)
{
    char modes[160];
    char dev[160];

    mode_list(modes, sizeof(modes));
    snprintf(dev, sizeof(dev),
             "{\"ids\":[\"pico_marquee_%s\"],\"name\":\"Pico Marquee\","
             "\"mf\":\"DIY\",\"mdl\":\"128x32 HUB75 marquee\"}",
             board_id);

    int n;
    switch (item)
    {
    case Q_DISC_LIGHT:
        snprintf(topic_buf, sizeof(topic_buf),
                 DISCOVERY_PREFIX "/light/pico_marquee_%s/light/config", board_id);
        n = snprintf(payload_buf, sizeof(payload_buf),
                     "{\"~\":\"%s\",\"name\":null,\"uniq_id\":\"pico_marquee_%s_light\","
                     "\"avty_t\":\"~/status\","
                     "\"cmd_t\":\"~/power/set\",\"stat_t\":\"~/power/state\","
                     "\"bri_cmd_t\":\"~/brightness/set\",\"bri_stat_t\":\"~/brightness/state\","
                     "\"fx_cmd_t\":\"~/mode/set\",\"fx_stat_t\":\"~/mode/state\","
                     "\"fx_list\":[%s],\"icon\":\"mdi:led-strip-variant\",\"dev\":%s}",
                     base, board_id, modes, dev);
        break;

    case Q_DISC_MODE:
        snprintf(topic_buf, sizeof(topic_buf),
                 DISCOVERY_PREFIX "/select/pico_marquee_%s/mode/config", board_id);
        n = snprintf(payload_buf, sizeof(payload_buf),
                     "{\"~\":\"%s\",\"name\":\"Mode\",\"uniq_id\":\"pico_marquee_%s_mode\","
                     "\"avty_t\":\"~/status\","
                     "\"cmd_t\":\"~/mode/set\",\"stat_t\":\"~/mode/state\","
                     "\"ops\":[%s],\"icon\":\"mdi:animation-play\",\"dev\":%s}",
                     base, board_id, modes, dev);
        break;

    case Q_DISC_TEXT:
        snprintf(topic_buf, sizeof(topic_buf),
                 DISCOVERY_PREFIX "/text/pico_marquee_%s/text/config", board_id);
        n = snprintf(payload_buf, sizeof(payload_buf),
                     "{\"~\":\"%s\",\"name\":\"Scroll text\",\"uniq_id\":\"pico_marquee_%s_text\","
                     "\"avty_t\":\"~/status\","
                     "\"cmd_t\":\"~/text/set\",\"stat_t\":\"~/text/state\","
                     "\"max\":%d,\"icon\":\"mdi:format-text\",\"dev\":%s}",
                     base, board_id, SCROLLER_MAX_LEN, dev);
        break;

    default:
        return false;
    }

    if (n < 0 || (size_t)n >= sizeof(payload_buf))
    {
        printf("[mqtt] Discovery payload too long\r\n");
        return false;
    }
    return true;
}

// Sends one queued item. Returns false if the client is out of room.
static bool send_item(uint32_t item)
{
    err_t err;

    switch (item)
    {
    case Q_SUB_CMD:
        snprintf(topic_buf, sizeof(topic_buf), "%s/+/set", base);
        err = mqtt_subscribe(client, topic_buf, 1, NULL, NULL);
        break;
    case Q_SUB_HA:
        err = mqtt_subscribe(client, DISCOVERY_PREFIX "/status", 1, NULL, NULL);
        break;
    case Q_ONLINE:
        err = publish(will_topic, "online", true);
        break;
    default:
        if (!build_discovery(item))
            return true; // drop it rather than retrying forever
        err = publish(topic_buf, payload_buf, true);
        break;
    }

    return err != ERR_MEM;
}

// Publishes whatever state changed since the last call. Stops at the first
// publish that does not fit and tries again next tick.
static void publish_state(void)
{
    char num[8];

    int demo = get_demo();
    if (demo != pub_demo)
    {
        if (publish_sub("mode/state", demo_name(demo)) == ERR_MEM)
            return;
        pub_demo = demo;
    }

    int on = display_on ? 1 : 0;
    if (on != pub_on)
    {
        if (publish_sub("power/state", on ? "ON" : "OFF") == ERR_MEM)
            return;
        pub_on = on;
    }

    int brightness = display_brightness;
    if (brightness != pub_brightness)
    {
        snprintf(num, sizeof(num), "%d", brightness);
        if (publish_sub("brightness/state", num) == ERR_MEM)
            return;
        pub_brightness = brightness;
    }

    const char *text = scroller_get_text();
    if (!pub_text_valid || strcmp(text, pub_text) != 0)
    {
        if (publish_sub("text/state", text) == ERR_MEM)
            return;
        strcpy(pub_text, text);
        pub_text_valid = true;
    }
}

// --- public API --------------------------------------------------------------

void ha_mqtt_start(void)
{
    if (board_id[0] == '\0')
    {
        pico_get_unique_board_id_string(board_id, sizeof(board_id));
        for (char *p = board_id; *p; p++)
            if (*p >= 'A' && *p <= 'F')
                *p = (char)(*p - 'A' + 'a');
        snprintf(base, sizeof(base), "pico-marquee/%s", board_id);
        snprintf(client_id, sizeof(client_id), "pico-marquee-%s", board_id);
        snprintf(will_topic, sizeof(will_topic), "%s/status", base);
    }

    // A fresh WiFi connection: try straight away instead of waiting out the
    // backoff. An existing session is left alone; if the WiFi drop killed it,
    // on_connection() moves back to idle and this path picks it up.
    if (state == MQ_IDLE)
    {
        retry_ms = RETRY_MIN_MS;
        retry_at = get_absolute_time();
    }
}

void ha_mqtt_task(void)
{
    if (board_id[0] == '\0')
        return; // never started

    switch (state)
    {
    case MQ_IDLE:
        if (wifi_is_connected() && time_reached(retry_at))
            start_connect();
        break;

    case MQ_CONNECTED:
        while (queue)
        {
            uint32_t item = queue & -queue; // lowest pending bit
            if (!send_item(item))
                return;
            queue &= ~item;
        }
        publish_state();
        break;

    default:
        break;
    }
}

#else /* no WiFi or no broker configured */

void ha_mqtt_start(void) {}
void ha_mqtt_task(void) {}

#endif
