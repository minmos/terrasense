#include <stdio.h>
#include "actors.h"
#include "gpio_switch.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "actor_fan.h"


void actors_init(sys_debug_led_t *debug_led)
{
    SYS_LOG("=== Initializing All Hardware Actors ===");

    gpio_switch_init(debug_led);
    actor_fan_init(debug_led);
    
    SYS_LOG("=== Actor Initialization Complete ===");
}