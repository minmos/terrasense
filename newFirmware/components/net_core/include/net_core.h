#pragma once

#include "sys_led.h" // Bring in the LED types

// Initializes the WiFi in Station mode and connects
void net_core_init(sys_debug_led_t *led_obj);