#include "sensor_binary.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "driver/gpio.h"

static sys_debug_led_t *status_led = NULL;

void sensor_binary_init(sys_debug_led_t *debug_led)
{
    status_led = debug_led;
#if HARDWARE_BINARY_ENABLED
    SYS_LOG("Initializing Binary Sensors. Configuring %d sensors...", HARDWARE_BINARY_COUNT);

    for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;  
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << HARDWARE_BINARY_CONFIG[i].gpio_pin);
        
        if (HARDWARE_BINARY_CONFIG[i].invert_logic) {
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        } else {
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        }
        
        gpio_config(&io_conf);
        SYS_LOG("Worker: Bound Binary Sensor '%s' to GPIO %d", HARDWARE_BINARY_CONFIG[i].name, HARDWARE_BINARY_CONFIG[i].gpio_pin);
    }
#endif
}

void sensor_binary_read(bool *output_states)
{
#if HARDWARE_BINARY_ENABLED
    for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
        int level = gpio_get_level(HARDWARE_BINARY_CONFIG[i].gpio_pin);
        
        if (HARDWARE_BINARY_CONFIG[i].invert_logic) {
            output_states[i] = (level == 0);
        } else {
            output_states[i] = (level == 1);
        }
    }
#endif
}