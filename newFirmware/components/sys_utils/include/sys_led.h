// components/sys_utils/include/sys_led.h
#pragma once

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_BUILTIN_LED    8
#define BUILTIN_LED_COUNT   1

#define SYS_LED_DEBUG_NETWORK_EN    1  // MQTT RX/TX flashes
#define SYS_LED_DEBUG_SENSOR_EN     1  // I2C/OneWire read flashes (Often noisy, keep 0 unless debugging)
#define SYS_LED_DEBUG_ACTOR_EN      1
#define SYS_PED_DEBUG_APP_LOGIC_EN  1
#if SYS_LED_DEBUG_NETWORK_EN
    #define LED_NOTIFY_NET(led, blinks)     sys_led_notify(led, LED_COLOR_BLUE, blinks)
#else
    #define LED_NOTIFY_NET(led, blinks)     do {} while(0)
#endif
// --- Sensor Diagnostics ---
#if SYS_LED_DEBUG_SENSOR_EN
    #define LED_NOTIFY_SENSOR(led, blinks)  sys_led_notify(led, LED_COLOR_CYAN, blinks)
#else
    #define LED_NOTIFY_SENSOR(led, blinks)  do {} while(0)
#endif
// --- Actor Diagnostics ---
#if SYS_LED_DEBUG_ACTOR_EN
    #define LED_NOTIFY_ACTOR(led, blinks)   sys_led_notify(led, LED_COLOR_WHITE, blinks)
#else
    #define LED_NOTIFY_ACTOR(led, blinks)   do {} while(0)
#endif
// --- App logic Diagnostics ---
#if SYS_PED_DEBUG_APP_LOGIC_EN
    #define LED_NOTIFY_APP_LOGIC(led, blinks)   sys_led_notify(led, LED_COLOR_WHITE, blinks)
#else
    #define LED_NOTIFY_APP_LOGIC(led, blinks)   do {} while(0)
#endif

// ============================================================================
// SYSTEM LED ARCHITECTURE & USAGE RULES
// ============================================================================
// This module separates the LED behavior into two distinct layers:
//
// 1. BASE SYSTEM STATES (The Background)
//    - Represents the overall health and mode of the device.
//    - Managed via `sys_led_set_state()`.
//    - Runs continuously. 
//
// 2. DIAGNOSTIC NOTIFICATIONS (The Foreground)
//    - Represents transient events (e.g., a relay clicking, a message arriving).
//    - Managed via `sys_led_notify()`.
//    - Temporarily overrides the Base State, then immediately returns to it.
//    - IGNORED when the Base State is SYS_LED_STATE_BOOTING.
//
// --- STANDARD COLOR PALETTE RULES ---
// Use these strict color meanings when calling sys_led_notify() to prevent 
// "blinkenlight" confusion:
// * WHITE:   Physical hardware actuation (Relay clicked ON/OFF).
// * BLUE:    Network RX (MQTT message received).
// * GREEN:   Network TX / Success (MQTT published, Sensor read successful).
// * CYAN:    Local Hardware Bus (I2C / OneWire scanning activity).
// * ORANGE:  Non-Fatal Warning (e.g., one sensor failed, but system is running).
// * RED:     Do NOT use for notifications! Red is reserved for SYS_LED_STATE_ERROR.
// ============================================================================

// --- Color Structure & Definitions ---
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sys_led_color_t;

#define LED_COLOR_YELLOW (sys_led_color_t){255, 180, 0}
#define LED_COLOR_ORANGE (sys_led_color_t){255, 80,  0}
#define LED_COLOR_GREEN  (sys_led_color_t){0, 255, 0}
#define LED_COLOR_CYAN   (sys_led_color_t){0, 255, 255}
#define LED_COLOR_BLUE   (sys_led_color_t){0, 0, 255}
#define LED_COLOR_RED    (sys_led_color_t){255, 0, 0}
#define LED_COLOR_PURPLE (sys_led_color_t){255, 0, 255}
#define LED_COLOR_WHITE  (sys_led_color_t){255, 255, 255}
#define LED_COLOR_BLACK  (sys_led_color_t){0, 0, 0}


// --- Base System States ---
typedef enum {
    SYS_LED_STATE_BOOTING,  // Solid Yellow: Initializing hardware/network.
    SYS_LED_STATE_OK_DAY,   // Dim Green: System healthy, normal operations.
    SYS_LED_STATE_OK_NIGHT, // Off/Ultra-dim: System healthy, stealth mode.
    SYS_LED_STATE_ERROR,    // Flashing Red: Fatal halt (e.g., No WiFi, I2C crash).
    SYS_LED_STATE_OFF       // Completely disabled.
} sys_led_state_t;


// --- LED Object Definition ---
typedef struct {
    led_strip_handle_t handle;
    uint16_t num_leds;
    volatile sys_led_state_t current_state; 
    TaskHandle_t task_handle;
    
    volatile uint8_t override_blinks_left;
    volatile bool override_trigger; 
    sys_led_color_t override_color; 
} sys_debug_led_t;


// ============================================================================
// PUBLIC API
// ============================================================================

/**
 *  Initializes the WS2812 LED hardware and spawns the background task.
 */
esp_err_t sys_led_init(sys_debug_led_t *led_obj);

/**
 *  Changes the continuous background health state of the system.
 *  Use this for long-term modes (Booting, OK, Error). 
 */
esp_err_t sys_led_set_state(sys_debug_led_t *led_obj, sys_led_state_t state);

/**
 *  Triggers a transient diagnostic flash.
 *  Use this for quick events (Relay clicks, MQTT messages). It will safely 
 *  interrupt the base state and return to it automatically.
 *  Flashes are ignored if the system is currently BOOTING.
 */
esp_err_t sys_led_notify(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t n_blinks);

/**
 * Only for internal task use only. Using this manually may conflict 
 * with the background state machine.
 */
esp_err_t sys_led_set_color(sys_debug_led_t *led_obj, sys_led_color_t color, uint8_t brightness_pct);



