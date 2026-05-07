// components/net_mqtt/net_mqtt.c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "esp_log.h"

#include "net_mqtt.h"
#include "sys_utils.h"


// State variables
static esp_mqtt_client_handle_t mqtt_client = NULL;
static sys_debug_led_t *builtin_status_led = NULL;
static mqtt_message_cb_t app_msg_cb = NULL;

// Helper to format the full topic: "BASE_TOPIC/subtopic"
static void get_full_topic(const char *subtopic, char *out_buffer, size_t max_len)
{
    snprintf(out_buffer, max_len, "%s/%s", CONFIG_MQTT_BASE_TOPIC, subtopic);
}

// --- MQTT Event Handler ---
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            SYS_LOG("Connected to MQTT Broker!");
            // Restore normal LED state if we recovered from an error
            if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_OK_DAY);
            break;

        case MQTT_EVENT_DISCONNECTED:
            SYS_LOG_WARN("Disconnected from MQTT Broker.");
            if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
            break;

        case MQTT_EVENT_DATA:
            // CRITICAL: MQTT strings are not null-terminated! We must copy them safely.
            if (app_msg_cb != NULL) {
                char topic[128];
                char payload[256];
                
                snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
                snprintf(payload, sizeof(payload), "%.*s", event->data_len, event->data);
                
                // Trigger the callback to let main.c handle the logic
                app_msg_cb(topic, payload);
                
                // Flash white once to indicate data reception!
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

// --- Public API ---

void net_mqtt_init(sys_debug_led_t *led_obj, mqtt_message_cb_t msg_cb)
{
    builtin_status_led = led_obj;
    app_msg_cb = msg_cb;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    SYS_ERR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL), "Failed to register MQTT events");
    
    SYS_LOG("MQTT Initialized. Base Topic: '%s'", CONFIG_MQTT_BASE_TOPIC);
}

void net_mqtt_start(void)
{
    if (mqtt_client != NULL) {
        SYS_LOG("Starting MQTT Client...");
        esp_mqtt_client_start(mqtt_client);
    }
}

/*
    any JSON formatting should be done beforehand
*/
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
    if (mqtt_client == NULL) return ESP_FAIL;

    char full_topic[128];
    get_full_topic(subtopic, full_topic, sizeof(full_topic));

    int msg_id = esp_mqtt_client_subscribe(mqtt_client, full_topic, qos);
    if (msg_id == -1) {
        SYS_LOG_ERR("Failed to subscribe to topic: %s", full_topic);
        return ESP_FAIL;
    }
    SYS_LOG("Subscribed to: %s", full_topic);
    return ESP_OK;
}

esp_err_t net_mqtt_publish_raw(const char *full_topic, const char *payload, int qos, int retain)
{
    if (mqtt_client == NULL) return ESP_FAIL;

    // Use strlen(payload) instead of 0 to ensure the exact JSON length is sent
    int msg_id = esp_mqtt_client_publish(mqtt_client, full_topic, payload, strlen(payload), qos, retain);
    
    if (msg_id == -1) {
        SYS_LOG_ERR("Failed to publish raw to topic: %s", full_topic);
        return ESP_FAIL;
    }
    
    // Quick green flash to indicate a successful configuration send
    if (builtin_status_led) sys_led_notify(builtin_status_led, LED_COLOR_RED, 1);
    return ESP_OK;
}