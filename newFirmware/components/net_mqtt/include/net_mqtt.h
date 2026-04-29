// components/net_mqtt/include/net_mqtt.h
#pragma once

#include "esp_err.h"
#include "sys_led.h"

// Define a callback signature for incoming MQTT messages
typedef void (*mqtt_message_cb_t)(const char *topic, const char *payload);

// Initialize the MQTT client, passing the LED for errors and the callback for messages
void net_mqtt_init(sys_debug_led_t *led_obj, mqtt_message_cb_t msg_cb);

// Starts the connection process (Call this AFTER WiFi connects)
void net_mqtt_start(void);

// Clever Pub/Sub: You only pass "temp" or "cmd", and the component prepends the Base Topic!
esp_err_t net_mqtt_publish(const char *subtopic, const char *payload, int qos, int retain);
esp_err_t net_mqtt_subscribe(const char *subtopic, int qos);

// for absolute topics like Home Assistant Discovery
esp_err_t net_mqtt_publish_raw(const char *full_topic, const char *payload, int qos, int retain);