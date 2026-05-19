#include <stdio.h>
#include "actors.h"
#include "gpio_switch.h"
#include "sys_config.h"
#include "sys_utils.h"

void actors_init(sys_debug_led_t *debug_led)
{
    SYS_LOG("=== Initializing All Hardware Actors ===");

    // 1. Initialize simple GPIO Switches (Relays)
    gpio_switch_init(debug_led);

    // 2. Future actors will go here!
    // pwm_dimmer_init(debug_led);
    // stepper_motor_init(debug_led);
    
    SYS_LOG("=== Actor Initialization Complete ===");
}