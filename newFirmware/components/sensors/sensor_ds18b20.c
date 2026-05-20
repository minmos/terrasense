#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "sys_utils.h"
#include "sys_config.h"

#include "sensor_ds18b20.h"
#include "sensors.h" 
#include "hass_discovery.h"


static sys_debug_led_t *status_led = NULL;
// Internal hardware state
static onewire_bus_handle_t bus = NULL;
// Initialize all handles to NULL (offline)
static ds18b20_device_handle_t ds18b20_handles[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY] = {NULL}; 

static int active_device_count = 0;
static int configured_device_count = 0;

void sensor_ds18b20_init(int pin, const ds18b20_target_t *targets, int target_count, sys_debug_led_t *debug_led) 
{
    status_led = debug_led;
    configured_device_count = target_count;
    SYS_LOG("Initializing DS18B20 Worker. Looking for %d specific sensors...", target_count);
    
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = pin,
        .flags.en_pull_up = true, 
    };
    onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = 10 };
    
    if (onewire_new_bus_rmt(&bus_config, &rmt_config, &bus) != ESP_OK) {
        SYS_LOG_ERR("Failed to initialize 1-Wire bus!");
        return;
    }

    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    
    // Search the bus for devices
    while (onewire_device_iter_get_next(iter, &next_onewire_device) == ESP_OK) {
        bool is_known_sensor = false;
        for (int i = 0; i < target_count; i++) {
            if (next_onewire_device.address == targets[i].rom_address) {
                is_known_sensor = true; // ← address is known regardless of what follows

                ds18b20_config_t ds_cfg = {};
                if (ds18b20_new_device_from_enumeration(&next_onewire_device, &ds_cfg, &ds18b20_handles[i]) == ESP_OK) {
                    SYS_LOG("Worker: Bound '%s' to Index %d", targets[i].name, i);
                    active_device_count++;
                } else {
                    SYS_LOG_ERR("Worker: Found '%s' but failed to create handle!", targets[i].name);
                }
                break; // correct — no need to check remaining targets
            }
        }
        if (!is_known_sensor) {
            SYS_LOG_WARN("Worker: Found UNKNOWN sensor! Add ROM to config: 0x%016llX",
                        next_onewire_device.address);
        }
    }
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
}

void sensor_ds18b20_read(float *output_array)
{
    // If no sensors are connected, fill buffer with invalid values and abort
    if (active_device_count == 0) {
        for (int i = 0; i < configured_device_count; i++) output_array[i] = SENSOR_VALUE_INVALID;
        return;
    }

    // Trigger all connected sensors to read the temperature simultaneously
    ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion_for_all(bus));
    
    // 12-bit resolution requires a maximum of 750ms
    vTaskDelay(pdMS_TO_TICKS(800));

    // Fetch the temperatures directly into their designated slots
    for (int i = 0; i < configured_device_count; i++) {
        if (ds18b20_handles[i] != NULL) {
            // Sensor is online, try to grab the reading
            if (ds18b20_get_temperature(ds18b20_handles[i], &output_array[i]) != ESP_OK) {
                output_array[i] = SENSOR_VALUE_INVALID; // I/O error during read
            }
        } else {
            // Sensor is physically disconnected or was never found at boot
            output_array[i] = SENSOR_VALUE_INVALID;
        }
    }
}