#pragma once
#include "sys_led.h"

// Triggers a background FreeRTOS task to download and apply the firmware
void sys_ota_start(sys_debug_led_t *led_obj, const char *url);