#include "gpio_switch.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "driver/gpio.h"
#include <string.h>

// Keep track of the current logical state in RAM
#if HARDWARE_SWITCH_ENABLED
static bool switch_states[HARDWARE_SWITCH_COUNT] = {false};
#endif

static sys_debug_led_t *status_led = NULL;

void gpio_switch_init(sys_debug_led_t *debug_led)
{
    status_led = debug_led; // Store for potential debug LED flashes later

#if HARDWARE_SWITCH_ENABLED
    SYS_LOG("Initializing GPIO Switches. Configuring %d switches...", HARDWARE_SWITCH_COUNT);

    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        // Determine physical level based on default logical state and active_high flag
        bool is_on = HARDWARE_SWITCH_CONFIG[i].default_state;
        int physical_level = is_on ? (HARDWARE_SWITCH_CONFIG[i].active_high ? 1 : 0)
                                   : (HARDWARE_SWITCH_CONFIG[i].active_high ? 0 : 1);

        // Configure the GPIO
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << HARDWARE_SWITCH_CONFIG[i].gpio_pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        // Set initial physical state BEFORE we finish config to prevent flickering
        gpio_set_level(HARDWARE_SWITCH_CONFIG[i].gpio_pin, physical_level);
        
        // Store logical state
        switch_states[i] = is_on;

        SYS_LOG("Worker: Bound Switch '%s' to GPIO %d (Default: %s)", 
                HARDWARE_SWITCH_CONFIG[i].name, 
                HARDWARE_SWITCH_CONFIG[i].gpio_pin, 
                is_on ? "ON" : "OFF");
    }
#endif
}

bool gpio_switch_set_state(int index, bool logical_state)
{
#if HARDWARE_SWITCH_ENABLED
    if (index < 0 || index >= HARDWARE_SWITCH_COUNT) return false;

    // Translate logical ON/OFF to physical HIGH/LOW
    int physical_level = logical_state ? (HARDWARE_SWITCH_CONFIG[index].active_high ? 1 : 0)
                                       : (HARDWARE_SWITCH_CONFIG[index].active_high ? 0 : 1);

    gpio_set_level(HARDWARE_SWITCH_CONFIG[index].gpio_pin, physical_level);
    switch_states[index] = logical_state;
    
    SYS_LOG("[ACTUATOR] %s -> %s", HARDWARE_SWITCH_CONFIG[index].name, logical_state ? "ON" : "OFF");
    return true;
#else
    return false;
#endif
}

bool gpio_switch_get_state(int index)
{
#if HARDWARE_SWITCH_ENABLED
    if (index < 0 || index >= HARDWARE_SWITCH_COUNT) return false;
    return switch_states[index];
#else
    return false;
#endif
}

int gpio_switch_get_index_by_id(const char *mqtt_device_id)
{
#if HARDWARE_SWITCH_ENABLED
    if (mqtt_device_id == NULL) return -1;
    
    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        if (strcmp(HARDWARE_SWITCH_CONFIG[i].mqtt_device_id, mqtt_device_id) == 0) {
            return i;
        }
    }
#endif
    return -1;
}