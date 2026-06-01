// components/net_mqtt/include/net_mqtt.h
#pragma once

#include "esp_err.h"
#include "sys_led.h"

typedef void (*mqtt_message_cb_t)(const char *topic, const char *payload);

void net_mqtt_init(sys_debug_led_t *led_obj, mqtt_message_cb_t msg_cb);

void net_mqtt_start(void);

esp_err_t net_mqtt_publish(const char *subtopic, const char *payload, int qos, int retain);
esp_err_t net_mqtt_subscribe(const char *subtopic, int qos);

esp_err_t net_mqtt_publish_raw(const char *full_topic, const char *payload, int qos, int retain);

bool net_mqtt_is_connected(void);