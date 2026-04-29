// components/sys_utils/include/sys_led.h
#pragma once

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- Color Structure & Definitions ---
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sys_led_color_t;

// Standard predefined colors using C99 compound literals
#define LED_COLOR_YELLOW (sys_led_color_t){255, 180, 0}
#define LED_COLOR_GREEN  (sys_led_color_t){0, 255, 0}
#define LED_COLOR_BLUE   (sys_led_color_t){0, 0, 255}
#define LED_COLOR_RED    (sys_led_color_t){255, 0, 0}
#define LED_COLOR_PURPLE (sys_led_color_t){255, 0, 255}
#define LED_COLOR_WHITE  (sys_led_color_t){255, 255, 255}
#define LED_COLOR_BLACK  (sys_led_color_t){0, 0, 0} // Used to turn the LED off

typedef enum {
    SYS_LED_STATE_BOOTING,  
    SYS_LED_STATE_OK_DAY,   
    SYS_LED_STATE_OK_NIGHT, 
    SYS_LED_STATE_ERROR,    
    SYS_LED_STATE_OFF       
} sys_led_state_t;

typedef struct {
    led_strip_handle_t handle;
    uint16_t num_leds;
    volatile sys_led_state_t current_state; 
    TaskHandle_t task_handle;

    // --- Notification Override Variables ---
    volatile uint8_t override_blinks_left;
    volatile bool override_trigger; 
    sys_led_color_t override_color; // Replaced the individual r, g, b variables
} sys_debug_led_t;

// --- Updated Methods ---
esp_err_t sys_led_init(sys_debug_led_t *led_obj, gpio_num_t gpio_num, uint16_t max_leds);
esp_err_t sys_led_set_state(sys_debug_led_t *led_obj, sys_led_state_t state);

// Replaced individual color parameters with the sys_led_color_t struct
esp_err_t sys_led_set_color(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t brightness_pct);
esp_err_t sys_led_notify(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t n_blinks);