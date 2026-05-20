#pragma once
#include "sensors.h" 
#include "sys_led.h"

// Notice the 'const' here. This allows it to accept the read-only flash array.
void sensor_ds18b20_init(int pin, const ds18b20_target_t *targets, int target_count, sys_debug_led_t *debug_led);

void sensor_ds18b20_read(float *output_array);