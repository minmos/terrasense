#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sys_led.h"

void actor_fan_init(sys_debug_led_t *debug_led);
bool actor_fan_set_speed(int index, uint8_t speed_percent);
uint8_t actor_fan_get_speed(int index);
uint32_t actor_fan_get_rpm(int index); 