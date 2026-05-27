#include "actor_fan.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h" // For high-res time tracking

static sys_debug_led_t *status_led = NULL;

#if HARDWARE_FAN_ENABLED
static uint8_t current_fan_speeds[HARDWARE_FAN_COUNT] = {0};
static pcnt_unit_handle_t pcnt_units[HARDWARE_FAN_COUNT] = {NULL};
static pcnt_channel_handle_t pcnt_channels[HARDWARE_FAN_COUNT] = {NULL};

// Track the exact microsecond timestamp of the last RPM read
static int64_t last_rpm_read_time[HARDWARE_FAN_COUNT] = {0};
#endif

void actor_fan_init(sys_debug_led_t *debug_led)
{
    status_led = debug_led;

#if HARDWARE_FAN_ENABLED
    SYS_LOG("Initializing Fans. Configuring %d fans...", HARDWARE_FAN_COUNT);

    for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
        
        // --- 1. HANDLE THE CONSTANT 12V FOR 4-PIN FANS ---
        if (HARDWARE_FAN_CONFIG[i].is_4pin) {
            // For a 4-pin fan, the MOSFET must be locked fully OPEN (100% ON) 
            // to provide a constant 12V supply. We configure it as a standard GPIO.
            gpio_config_t power_conf = {
                .pin_bit_mask = (1ULL << HARDWARE_FAN_CONFIG[i].power_pin),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config(&power_conf);
            gpio_set_level(HARDWARE_FAN_CONFIG[i].power_pin, 1); // Turn 12V ON immediately
            
            SYS_LOG("Worker: Locked Fan '%s' MOSFET (GPIO %d) to constant 12V ON.", 
                    HARDWARE_FAN_CONFIG[i].name, HARDWARE_FAN_CONFIG[i].power_pin);
        }

        // --- 2. LEDC PWM CONFIGURATION ---
        uint32_t freq_hz = HARDWARE_FAN_CONFIG[i].is_4pin ? 25000 : 1000;
        ledc_timer_t timer_num = HARDWARE_FAN_CONFIG[i].is_4pin ? LEDC_TIMER_0 : LEDC_TIMER_1;

        // DYNAMIC ROUTING: Choose which pin actually receives the PWM signal!
        int active_pwm_pin = HARDWARE_FAN_CONFIG[i].is_4pin ? 
                             HARDWARE_FAN_CONFIG[i].pwm_pin : 
                             HARDWARE_FAN_CONFIG[i].power_pin;

        ledc_timer_config_t timer_conf = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .timer_num        = timer_num,
            .duty_resolution  = LEDC_TIMER_8_BIT,
            .freq_hz          = freq_hz,
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

        ledc_channel_config_t channel_conf = {
            .gpio_num       = active_pwm_pin, // Routes to Logic or MOSFET automatically
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = LEDC_CHANNEL_0 + i,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = timer_num,
            .duty           = 0,
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
        current_fan_speeds[i] = 0;

        // --- 3. PCNT TACHOMETER CONFIGURATION ---
        if (HARDWARE_FAN_CONFIG[i].tach_pin != FAN_UNUSED_PIN) {
            gpio_set_pull_mode(HARDWARE_FAN_CONFIG[i].tach_pin, GPIO_PULLUP_ONLY);

            pcnt_unit_config_t unit_config = {
                .high_limit = 20000, 
                .low_limit = -100,
            };
            ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_units[i]));

            pcnt_glitch_filter_config_t filter_config = {
                .max_glitch_ns = 1000, 
            };
            ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_units[i], &filter_config));

            pcnt_chan_config_t chan_config = {
                .edge_gpio_num = HARDWARE_FAN_CONFIG[i].tach_pin,
                .level_gpio_num = -1, 
            };
            ESP_ERROR_CHECK(pcnt_new_channel(pcnt_units[i], &chan_config, &pcnt_channels[i]));

            ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_channels[i], PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
            
            ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_units[i]));
            ESP_ERROR_CHECK(pcnt_unit_start(pcnt_units[i]));

            last_rpm_read_time[i] = esp_timer_get_time();
        }

        SYS_LOG("Worker: Bound %d-Pin Fan '%s' [Active PWM Pin: %d | Tach Pin: %d]", 
                HARDWARE_FAN_CONFIG[i].is_4pin ? 4 : 3,
                HARDWARE_FAN_CONFIG[i].name, 
                active_pwm_pin,
                HARDWARE_FAN_CONFIG[i].tach_pin);
                
        actor_fan_set_speed(i, 0);
    }
#endif
}


bool actor_fan_set_speed(int index, uint8_t speed_percent)
{
#if HARDWARE_FAN_ENABLED
    if (index < 0 || index >= HARDWARE_FAN_COUNT) return false;
    if (speed_percent > 100) speed_percent = 100;

    // 1. Save the Home Assistant target immediately so get_speed() reports correctly
    current_fan_speeds[index] = speed_percent;

    // 2. Hardware Dead-Zone Mapping (Hardcoded!)
    uint8_t hw_target = 0;
    
    if (speed_percent > 0) {
        // If it's a 4-pin fan, the minimum is 15%. If 3-pin, it's 40%.
        uint8_t min_speed = HARDWARE_FAN_CONFIG[index].is_4pin ? 15 : 40;
        
        // Map 1-100 input into the remaining working percentage gap
        hw_target = min_speed + ((speed_percent * (100 - min_speed)) / 100);
    }

    // 3. Calculate normal 8-bit duty cycle (0-255) using the MAPPED hardware target
    uint32_t duty = (hw_target * 255) / 100;

    // 4. Automatic Inversion for 4-pin fans
    if (HARDWARE_FAN_CONFIG[index].is_4pin) {
        duty = 255 - duty; 
    }

    // 5. Apply to hardware
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + index, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + index);
    
    if (status_led != NULL) {
        LED_NOTIFY_ACTOR(status_led, 1);
    }
    
    return true;
#else
    return false;
#endif
}

uint32_t actor_fan_get_rpm(int index)
{
#if HARDWARE_FAN_ENABLED
    if (index < 0 || index >= HARDWARE_FAN_COUNT) return 0;
    if (pcnt_units[index] == NULL) return 0; 

    // 1. Calculate exactly how much time has passed since the last call
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_us = current_time - last_rpm_read_time[index];
    last_rpm_read_time[index] = current_time;

    // Prevent divide-by-zero if called too rapidly
    if (elapsed_us == 0) return 0; 

    // 2. Read and clear the hardware pulse counter
    int pulse_count = 0;
    pcnt_unit_get_count(pcnt_units[index], &pulse_count);
    pcnt_unit_clear_count(pcnt_units[index]);

    // 3. Dynamic RPM Math
    // RPM = (Pulses / 2 pulses per rev) * (60 seconds / elapsed seconds)
    // Converted for microseconds: RPM = (Pulses * 30,000,000) / elapsed_us
    uint64_t rpm = ((uint64_t)pulse_count * 30000000ULL) / elapsed_us;
    
    return (uint32_t)rpm;
#else
    return 0;
#endif
}

uint8_t actor_fan_get_speed(int index)
{
#if HARDWARE_FAN_ENABLED
    if (index < 0 || index >= HARDWARE_FAN_COUNT) return 0;
    return current_fan_speeds[index];
#else
    return 0;
#endif
}