#pragma once

#include "sys_led.h"

void app_logic_init(sys_debug_led_t *led);
void app_logic_handle_mqtt(const char *topic, const char *payload);