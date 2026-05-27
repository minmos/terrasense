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
    int tach_pin;      // The GPIO connected to the Tachometer (RPM). Set to -1 if not used.
    int pwm_pin;       // The GPIO connected to the Fan's PWM wire (or the MOSFET gate)
    bool is_4pin;      // true = Uses 25kHz standard. false = Uses 1kHz for MOSFET switching.
} fan_target_t;

// API
void actors_init(sys_debug_led_t *debug_led);
