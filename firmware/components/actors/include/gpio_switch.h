#pragma once
#include <stdbool.h>
#include "sys_led.h"

void gpio_switch_init(sys_debug_led_t *debug_led);
bool gpio_switch_set_state(int index, bool state);
bool gpio_switch_get_state(int index);
int gpio_switch_get_index_by_id(const char *mqtt_device_id);