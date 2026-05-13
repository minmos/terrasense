#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_utils.h"
#include "sys_config.h"
#include "sensor_sht3x.h"
#include "sht3x.h"
#include <string.h>

// Internal hardware state
static sht3x_t sht3x_handles[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
static bool sht3x_active[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY] = {false};

static int configured_sht3x_count = 0;
static const sht3x_target_t *sht_configs = NULL;

void sensor_sht3x_init(int i2c_port, int sda_pin, int scl_pin, const sht3x_target_t *targets, int target_count)
{
    configured_sht3x_count = target_count;
    sht_configs = targets;
    
    SYS_LOG("Initializing SHT3X Worker. Configuring %d sensors...", target_count);

    for (int i = 0; i < target_count; i++) {
        memset(&sht3x_handles[i], 0, sizeof(sht3x_t));
        
        // Initialize the device descriptor
        ESP_ERROR_CHECK(sht3x_init_desc(&sht3x_handles[i], targets[i].i2c_address, i2c_port, sda_pin, scl_pin));
        
        // Future Multiplexer logic check during init
        if (targets[i].mux_channel >= 0) {
            SYS_LOG("Worker: SHT3X '%s' configured on MUX channel %d (Preparation mode)", targets[i].name, targets[i].mux_channel);
            // TODO: Once TCA9548A is added, you will switch the mux here to initialize the sensor
        }

        esp_err_t res = sht3x_init(&sht3x_handles[i]);
        if (res == ESP_OK) {
            sht3x_active[i] = true;
            SYS_LOG("Worker: Bound '%s' to Index %d", targets[i].name, i);
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

        // ==========================================================
        // MULTIPLEXER SWITCHING LOGIC WILL GO HERE
        // ==========================================================
        if (sht_configs[i].mux_channel >= 0) {
            // TODO: Call tca9548_set_channels(&mux_dev, (1 << sht_configs[i].mux_channel));
            // This ensures the I2C bus routes to the correct physical sensor before reading
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