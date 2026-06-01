#pragma once
#include "sys_led.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int gpio_pin;
    bool active_high;          
    bool default_state;        
} switch_target_t;

typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int power_pin;
    int tach_pin;      
    int pwm_pin;       
    bool is_4pin;      
} fan_target_t;

// API
void actors_init(sys_debug_led_t *debug_led);
