#pragma once
#include "sensors.h" 
#include "sys_led.h"

void sensor_sht3x_init(int i2c_port, int sda_pin, int scl_pin, const sht3x_target_t *targets, int target_count, sys_debug_led_t *debug_led);
void sensor_sht3x_read(float *output_temps, float *output_hums);