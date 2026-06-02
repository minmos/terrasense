#pragma once
#include <stdbool.h>
#include "sys_led.h"

void sensor_binary_init(sys_debug_led_t *debug_led);
void sensor_binary_read(bool *output_states);