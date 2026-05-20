#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_utils.h"
#include "sys_config.h"
#include "sensor_sht3x.h"
#include "sht3x.h"
#include "tca9548.h" // Added the MUX library
#include <string.h>


static sys_debug_led_t *status_led = NULL;
// Internal hardware state
static sht3x_t sht3x_handles[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
static bool sht3x_active[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY] = {false};
static i2c_dev_t mux_dev; // The multiplexer device descriptor

static int configured_sht3x_count = 0;
static const sht3x_target_t *sht_configs = NULL;

void sensor_sht3x_init(int i2c_port, int sda_pin, int scl_pin, const sht3x_target_t *targets, int target_count, sys_debug_led_t *debug_led)
{
    status_led = debug_led;
    configured_sht3x_count = target_count;
    sht_configs = targets;
    
    SYS_LOG("Initializing SHT3X Worker and MUX. Configuring %d sensors...", target_count);

    // 1. Initialize the Multiplexer descriptor
    memset(&mux_dev, 0, sizeof(i2c_dev_t));
    esp_err_t mux_res = tca9548_init_desc(&mux_dev, MUX_I2C_ADDRESS, i2c_port, sda_pin, scl_pin);
    if (mux_res != ESP_OK) {
        SYS_LOG_ERR("Failed to initialize TCA9548A Multiplexer descriptor!");
        return; // If the MUX fails, we can't talk to any sensors
    }

    // 2. Initialize each sensor
    for (int i = 0; i < target_count; i++) {
        memset(&sht3x_handles[i], 0, sizeof(sht3x_t));
        
        // Setup the sensor descriptor
        ESP_ERROR_CHECK(sht3x_init_desc(&sht3x_handles[i], SHT35_I2C_ADDR, i2c_port, sda_pin, scl_pin));
        
        SYS_LOG("Worker: Connecting MUX to channel %d for '%s'", targets[i].mux_channel, targets[i].name);
        
        // SWITCH MUX CHANNEL BEFORE INIT
        // tca9548_set_channels takes a bitmask. Channel 0 = 1<<0, Channel 1 = 1<<1, etc.
        esp_err_t channel_res = tca9548_set_channels(&mux_dev, (1 << targets[i].mux_channel));
        
        if (channel_res != ESP_OK) {
            SYS_LOG_ERR("Failed to switch MUX to channel %d. Skipping sensor.", targets[i].mux_channel);
            sht3x_active[i] = false;
            continue;
        }

        // Now that the I2C bus is routed to the physical sensor, initialize it
        esp_err_t res = sht3x_init(&sht3x_handles[i]);
        if (res == ESP_OK) {
            sht3x_active[i] = true;
            SYS_LOG("Worker: Successfully Bound '%s' to Index %d", targets[i].name, i);
        } else {
            sht3x_active[i] = false;
            SYS_LOG_ERR("Worker: Failed to initialize '%s' (Mux %d). Check wiring!", targets[i].name, targets[i].mux_channel);
        }
    }
}

void sensor_sht3x_read(float *output_temps, float *output_hums)
{
    for (int i = 0; i < configured_sht3x_count; i++) {
        if (!sht3x_active[i]) {
            output_temps[i] = SENSOR_VALUE_INVALID;
            output_hums[i]  = SENSOR_VALUE_INVALID;
            continue;
        }

        // Route the I2C bus to the correct physical sensor
        esp_err_t mux_res = tca9548_set_channels(&mux_dev, (1 << sht_configs[i].mux_channel));
        
        if (mux_res != ESP_OK) {
            output_temps[i] = SENSOR_VALUE_INVALID;
            output_hums[i]  = SENSOR_VALUE_INVALID;
            continue; // Skip reading if MUX failed to switch
        }

        // Read the sensor
        float t, h;
        esp_err_t res = sht3x_measure(&sht3x_handles[i], &t, &h);
        
        if (res == ESP_OK) {
            output_temps[i] = t;
            output_hums[i]  = h;
        } else {
            output_temps[i] = SENSOR_VALUE_INVALID;
            output_hums[i]  = SENSOR_VALUE_INVALID;
        }
    }
}