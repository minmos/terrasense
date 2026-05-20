// components/sys_utils/sys_led.c
#include "sys_led.h"
#include "sys_utils.h" 
#include "sys_config.h" 

static const char *TAG __attribute__((unused)) = "SYS_LED";

static void sys_led_task(void *arg)
{
    sys_debug_led_t *led_obj = (sys_debug_led_t *)arg;
    sys_led_state_t last_state = SYS_LED_STATE_OFF;
    
    uint32_t tick_count = 0;
    bool toggle_flag = false;

    uint32_t override_tick = 0;
    bool was_overridden = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 ticks per second

        // --- 1. HANDLE DEBUG OVERRIDES ---
        if (led_obj->override_blinks_left > 0) {
            was_overridden = true;

            if (led_obj->override_trigger) {
                override_tick = 0;
                led_obj->override_trigger = false;
            }

            // Quick 100ms ON, 100ms OFF pattern
            if (override_tick == 0) {
                sys_led_set_color(led_obj, led_obj->override_color, 40);
            } else if (override_tick == 1) {
                sys_led_set_color(led_obj, LED_COLOR_BLACK, 0);
            }

            override_tick++;
            
            if (override_tick >= 2) {
                override_tick = 0;
                led_obj->override_blinks_left--; 
            }
            continue; 
        }

        // --- 2. NORMAL STATE MACHINE ---
        tick_count++;
        
        bool state_changed = (led_obj->current_state != last_state) || was_overridden;
        
        if (state_changed) {
            tick_count = 0;          
            toggle_flag = true;      
            last_state = led_obj->current_state;
            was_overridden = false;  
        }

        switch (led_obj->current_state) {
            case SYS_LED_STATE_BOOTING:
                // Solid Yellow to indicate initialization
                if (state_changed) sys_led_set_color(led_obj, LED_COLOR_YELLOW, 20);
                break;

            case SYS_LED_STATE_ERROR:
                // Aggressive Fast Red Flash
                if (state_changed || tick_count % 2 == 0) {
                    if (toggle_flag) sys_led_set_color(led_obj, LED_COLOR_RED, 80);
                    else sys_led_set_color(led_obj, LED_COLOR_BLACK, 0);
                    toggle_flag = !toggle_flag;
                }
                break;

            case SYS_LED_STATE_OK_DAY:
                // Solid Dim Green
                if (state_changed) sys_led_set_color(led_obj, LED_COLOR_GREEN, 5);
                break;

            case SYS_LED_STATE_OK_NIGHT:
                // Off (or extremely dim blue) so it doesn't disturb the terrarium
                if (state_changed) sys_led_set_color(led_obj, LED_COLOR_BLACK, 0);
                break;

            case SYS_LED_STATE_OFF:
            default:
                if (state_changed) sys_led_set_color(led_obj, LED_COLOR_BLACK, 0);
                break;
        }
    }
}

esp_err_t sys_led_init(sys_debug_led_t *led_obj)
{
    if (led_obj == NULL) return ESP_ERR_INVALID_ARG;

    led_obj->num_leds = BUILTIN_LED_COUNT;
    led_obj->current_state = SYS_LED_STATE_OFF; 

    led_strip_config_t strip_config = {
        .strip_gpio_num = GPIO_BUILTIN_LED,
        .max_leds = BUILTIN_LED_COUNT, 
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, 
        .flags.with_dma = false,           
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_obj->handle);
    if (err == ESP_OK) {
        led_strip_clear(led_obj->handle);
        SYS_LOG("Debug LED hardware initialized on GPIO %d", GPIO_BUILTIN_LED);
        xTaskCreate(sys_led_task, "sys_led_daemon", 2048, led_obj, 2, &led_obj->task_handle);
    }
    return err;
}

esp_err_t sys_led_set_color(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t brightness_pct)
{
    if (led_obj == NULL || led_obj->handle == NULL) return ESP_ERR_INVALID_STATE;
    if (brightness_pct > 100) brightness_pct = 100;

    uint8_t scaled_r = (color.r * brightness_pct) / 100;
    uint8_t scaled_g = (color.g * brightness_pct) / 100;
    uint8_t scaled_b = (color.b * brightness_pct) / 100;

    for (int i = 0; i < led_obj->num_leds; i++) {
        led_strip_set_pixel(led_obj->handle, i, scaled_r, scaled_g, scaled_b);
    }
    return led_strip_refresh(led_obj->handle);
}

esp_err_t sys_led_set_state(sys_debug_led_t *led_obj, sys_led_state_t state)
{
    if (led_obj == NULL) return ESP_ERR_INVALID_ARG;
    led_obj->current_state = state;
    return ESP_OK;
}

esp_err_t sys_led_notify(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t n_blinks)
{
    if (led_obj == NULL) return ESP_ERR_INVALID_ARG;

    // --- BOOT PROTECTION ---
    // Do not allow transient diagnostic flashes during the boot phase.
    // If a fatal error occurs during boot, the system must call 
    // sys_led_set_state(led, SYS_LED_STATE_ERROR) instead of using a notification.
    if (led_obj->current_state == SYS_LED_STATE_BOOTING) {
        return ESP_ERR_INVALID_STATE; 
    }

#if SYS_LED_DEBUG_MODE_ENABLED
    led_obj->override_color = color;
    led_obj->override_trigger = true;
    led_obj->override_blinks_left = n_blinks;
    return ESP_OK;
#else
    // Silently ignore debug flashes in production mode
    return ESP_OK; 
#endif
}