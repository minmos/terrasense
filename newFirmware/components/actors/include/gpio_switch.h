#pragma once
#include <stdbool.h>
#include "sys_led.h"

void gpio_switch_init(sys_debug_led_t *debug_led);

// Set logical state: true = ON, false = OFF
bool gpio_switch_set_state(int index, bool state);

// Get current logical state
bool gpio_switch_get_state(int index);

// Lookup index by MQTT device ID (Useful for incoming MQTT commands)
int gpio_switch_get_index_by_id(const char *mqtt_device_id);