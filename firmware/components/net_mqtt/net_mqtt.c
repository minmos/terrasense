// components/net_mqtt/net_mqtt.c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "esp_log.h"

#include "net_mqtt.h"
#include "sys_utils.h"
#include "sys_config.h"

static esp_mqtt_client_handle_t mqtt_client = NULL;
static sys_debug_led_t *builtin_status_led = NULL;
static mqtt_message_cb_t app_msg_cb = NULL;
static bool mqtt_connected = false;

#define MAX_MQTT_SUBSCRIPTIONS 16
static char registered_subtopics[MAX_MQTT_SUBSCRIPTIONS][128];
static int registered_subtopics_count = 0;

static void get_full_topic(const char *subtopic, char *out_buffer, size_t max_len)
{
    snprintf(out_buffer, max_len, "%s/%s", MQTT_BASE_TOPIC, subtopic);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true; 
            SYS_LOG("Connected to MQTT Broker!");
            esp_mqtt_client_publish(mqtt_client, MQTT_AVAILABILITY_TOPIC, "online", 0, 1, 1); // to let the mqtt broker know that we are alive
            
            // Re-subscribe to all registered subtopics
            for (int i = 0; i < registered_subtopics_count; i++) {
                char full_topic[128];
                get_full_topic(registered_subtopics[i], full_topic, sizeof(full_topic));
                int msg_id = esp_mqtt_client_subscribe(mqtt_client, full_topic, 1);
                if (msg_id != -1) {
                    SYS_LOG("Subscribed to: %s", full_topic);
                } else {
                    SYS_LOG_ERR("Failed to subscribe to: %s", full_topic);
                }
            }
            if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_OK_DAY);
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false; 
            SYS_LOG_WARN("Disconnected from MQTT Broker.");
            if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
            break;

        case MQTT_EVENT_DATA:
            if (app_msg_cb != NULL) {
                char topic[128];
                char payload[256];
                snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
                snprintf(payload, sizeof(payload), "%.*s", event->data_len, event->data);
                app_msg_cb(topic, payload);
                if (builtin_status_led) sys_led_notify(builtin_status_led, LED_COLOR_BLUE, 4);
            }
            break;

        case MQTT_EVENT_ERROR:
            SYS_LOG_ERR("MQTT Event Error");
            if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
            break;

        default:
            break;
    }
}


void net_mqtt_init(sys_debug_led_t *led_obj, mqtt_message_cb_t msg_cb)
{
    builtin_status_led = led_obj;
    app_msg_cb = msg_cb;
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    SYS_ERR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL), "Failed to register MQTT events");
    SYS_LOG("MQTT Initialized. Base Topic: '%s'", MQTT_BASE_TOPIC);
}

void net_mqtt_start(void)
{
    if (mqtt_client != NULL) {
        SYS_LOG("Starting MQTT Client...");
        esp_mqtt_client_start(mqtt_client);
    }
}

esp_err_t net_mqtt_publish(const char *subtopic, const char *payload, int qos, int retain)
{
    if (mqtt_client == NULL) return ESP_FAIL;

    char full_topic[128];
    get_full_topic(subtopic, full_topic, sizeof(full_topic));

    int msg_id = esp_mqtt_client_publish(mqtt_client, full_topic, payload, 0, qos, retain);
    if (msg_id == -1) {
        SYS_LOG_ERR("Failed to publish to topic: %s", full_topic);
        return ESP_FAIL;
    }
    if (builtin_status_led) sys_led_notify(builtin_status_led, LED_COLOR_BLUE, 2);
    return ESP_OK;
}

esp_err_t net_mqtt_subscribe(const char *subtopic, int qos)
{
    if (subtopic == NULL) return ESP_FAIL;

    // Check if already registered
    bool found = false;
    for (int i = 0; i < registered_subtopics_count; i++) {
        if (strcmp(registered_subtopics[i], subtopic) == 0) {
            found = true;
            break;
        }
    }

    // Add to registry if not already present
    if (!found) {
        if (registered_subtopics_count < MAX_MQTT_SUBSCRIPTIONS) {
            strncpy(registered_subtopics[registered_subtopics_count], subtopic, sizeof(registered_subtopics[0]) - 1);
            registered_subtopics[registered_subtopics_count][sizeof(registered_subtopics[0]) - 1] = '\0';
            registered_subtopics_count++;
        } else {
            SYS_LOG_ERR("MQTT Subscription registry is full! Cannot subscribe to: %s", subtopic);
            return ESP_FAIL;
        }
    }

    // If already connected, perform the subscription immediately
    if (mqtt_connected && mqtt_client != NULL) {
        char full_topic[128];
        get_full_topic(subtopic, full_topic, sizeof(full_topic));
        int msg_id = esp_mqtt_client_subscribe(mqtt_client, full_topic, qos);
        if (msg_id == -1) {
            SYS_LOG_ERR("Failed to subscribe to topic: %s", full_topic);
            return ESP_FAIL;
        }
        SYS_LOG("Subscribed to: %s", full_topic);
    }

    return ESP_OK;
}

esp_err_t net_mqtt_publish_raw(const char *full_topic, const char *payload, int qos, int retain)
{
    if (mqtt_client == NULL) return ESP_FAIL;

    int msg_id = esp_mqtt_client_publish(mqtt_client, full_topic, payload, strlen(payload), qos, retain);
    
    if (msg_id == -1) {
        SYS_LOG_ERR("Failed to publish raw to topic: %s", full_topic);
        return ESP_FAIL;
    }
    if (builtin_status_led) sys_led_notify(builtin_status_led, LED_COLOR_BLUE, 1);
    return ESP_OK;
}

bool net_mqtt_is_connected(void)
{
    return mqtt_connected;
}