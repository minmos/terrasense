#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// System & Core Components
#include "hardware_pin_config.h"
#include "sys_utils.h"
#include "sys_led.h" 
#include "sys_ota.h" 

// Network Components
#include "net_core.h"
#include "net_mqtt.h" 

// Application Logic (Future includes)
#include "sensors.h"  // For I2C (BME280) and ADC (Soil Moisture)
// #include "app_relays.h"   // For GPIO (Heater, Pump, Vents)
// #include "app_control.h"  // The brain: turns on heater if temp is low

// Global Handles
sys_debug_led_t builtin_status_led;
#define GPIO_BUILTIN_LED 8

// #include "onewire_bus.h"
// #include "ds18b20.h"

// #define ONEWIRE_BUS_GPIO 18
// #define EXAMPLE_ONEWIRE_BUS_GPIO    18
// #define EXAMPLE_ONEWIRE_MAX_DS18B20 2

// ============================================================================
// 1. MQTT ROUTING DISPATCHER
// ============================================================================
// As this grows, you will move this entire function to an `app_mqtt_router.c` file.
// For now, it lives here, acting as the switchboard for incoming cloud commands.
static void mqtt_router_callback(const char *topic, const char *payload)
{
    SYS_LOG("INCOMING MQTT -> Topic: %s | Payload: %s", topic, payload);
    
    // --- System Commands ---
    if (strstr(topic, "cmd/update") != NULL) {
        SYS_LOG("OTA Command Received!");
        sys_ota_start(&builtin_status_led, payload);
        return;
    }

    // --- Actuator Commands (Future) ---
    /*
    if (strstr(topic, "cmd/heater") != NULL) {
        // e.g., app_relays_set_heater(atoi(payload));
        return;
    }
    if (strstr(topic, "cmd/pump") != NULL) {
        // e.g., app_relays_trigger_watering();
        return;
    }
    */
    
    // --- Configuration Commands (Future) ---
    /*
    if (strstr(topic, "cmd/target_temp") != NULL) {
        // e.g., app_control_set_target_temp(atof(payload));
        return;
    }
    */
}

// ============================================================================
// MAIN APPLICATION ENTRY
// ============================================================================
void app_main(void)
{
    // Give hardware power-rails a moment to stabilize before slamming the bus
    vTaskDelay(pdMS_TO_TICKS(500)); 

    // ------------------------------------------------------------------------
    // PHASE 1: Core System Initialization
    // ------------------------------------------------------------------------
    sys_led_init(&builtin_status_led, GPIO_BUILTIN_LED, 1); 
    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_BOOTING);

    SYS_LOG("=========================================");
    SYS_LOG("Terracotta Greenhouse Controller Booting!");
    SYS_LOG("=========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    SYS_ERR_CHECK(ret, "Failed to initialize NVS");

    // ------------------------------------------------------------------------
    // PHASE 2: Hardware Peripherals & App State (Future)
    // ------------------------------------------------------------------------
    // SYS_LOG("Initializing Hardware Peripherals...");
    // app_sensors_init();   // Sets up I2C bus and ADC pins
    // app_relays_init();    // Configures GPIO pins to output and sets them LOW
    // app_control_init();   // Loads saved target temps from NVS
    sensors_init(ONEWIRE_BUS_GPIO, HARDWARE_DS18B20_CONFIG, HARDWARE_DS18B20_COUNT);

    // ------------------------------------------------------------------------
    // PHASE 3: Network & Middleware Initialization
    // ------------------------------------------------------------------------
    SYS_LOG("Initializing Networking Stack...");
    net_core_init(&builtin_status_led);
    
    // Wait for IP (In the future, use FreeRTOS Event Groups instead of a blind delay)
    vTaskDelay(pdMS_TO_TICKS(5000));

    net_mqtt_init(&builtin_status_led, mqtt_router_callback);
    net_mqtt_start();
    vTaskDelay(pdMS_TO_TICKS(2000)); 

    // Subscribe to all relevant command topics
    net_mqtt_subscribe("cmd/update", 1);
    // net_mqtt_subscribe("cmd/heater", 1);
    // net_mqtt_subscribe("cmd/target_temp", 1);

    SYS_LOG("Boot sequence complete. Entering standard operation.");
    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_OK_DAY);

    // ------------------------------------------------------------------------
    // PHASE 4: Spawn Background Application Tasks
    // ------------------------------------------------------------------------
    // Instead of doing work in the main loop, we spawn dedicated tasks.
    // This allows the ESP32 to run sensors on Core 0 and networking on Core 1!
    
    // xTaskCreate(app_sensors_read_task, "sensor_task", 4096, NULL, 5, NULL);
    // xTaskCreate(app_control_logic_task, "control_task", 4096, NULL, 4, NULL);

    // ------------------------------------------------------------------------
    // PHASE 5: The Main Loop (System Monitor)
    // ------------------------------------------------------------------------
    // Since all heavy lifting is now handled by FreeRTOS tasks and callbacks,
    // the main loop is relegated to a slow-ticking system monitor or watchdog.

    // SYS_LOG("Initializing 1-Wire Bus on GPIO %d...", ONEWIRE_BUS_GPIO);
    
    
    // // install 1-wire bus
    // onewire_bus_handle_t bus = NULL;
    // onewire_bus_config_t bus_config = {
    //     .bus_gpio_num = EXAMPLE_ONEWIRE_BUS_GPIO,
    //     .flags = {
    //         .en_pull_up = true, // enable the internal pull-up resistor in case the external device didn't have one
    //     }
    // };
    // onewire_bus_rmt_config_t rmt_config = {
    //     .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
    // };
    // ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    // int ds18b20_device_num = 0;
    // ds18b20_device_handle_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
    // onewire_device_iter_handle_t iter = NULL;
    // onewire_device_t next_onewire_device;
    // esp_err_t search_result = ESP_OK;

    // // create 1-wire device iterator, which is used for device search
    // ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    // SYS_LOG("Device iterator created, start searching...");
    // do {
    //     search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
    //     if (search_result == ESP_OK) { // found a new device, let's check if we can upgrade it to a DS18B20
    //         ds18b20_config_t ds_cfg = {};
    //         onewire_device_address_t address;
    //         // check if the device is a DS18B20, if so, return the ds18b20 handle
    //         if (ds18b20_new_device_from_enumeration(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK) {
    //             ds18b20_get_device_address(ds18b20s[ds18b20_device_num], &address);
    //             SYS_LOG( "Found a DS18B20[%d], address: %016llX, addr_in_hex: %p", ds18b20_device_num, address, address);
    //             ds18b20_device_num++;
    //         } else {
    //             SYS_LOG( "Found an unknown device, address: %016llX", next_onewire_device.address);
    //         }
    //     }
    // } while (search_result != ESP_ERR_NOT_FOUND);
    // ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    // SYS_LOG( "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);

    
    sensor_data_t current_env_data;
    while (1) {
        // Safely fetch a copy of the latest background readings
        if (sensors_get_data(&current_env_data)) {
            SYS_LOG("--- ENVIRONMENT UPDATE ---");
            
            for (int i = 0; i < current_env_data.ds18b20_count; i++) {
                float temp = current_env_data.ds18b20_temps[i];
                
                if (temp != SENSOR_VALUE_INVALID) {
                    SYS_LOG("%s: %.2f °C", HARDWARE_DS18B20_CONFIG[i].name, temp);
                } else {
                    SYS_LOG_ERR("%s: OFFLINE", HARDWARE_DS18B20_CONFIG[i].name);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));
    }

    // uint32_t uptime_minutes = 0;
    // while (1) {
    //     ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion_for_all(bus));
    //     float temperature;
    //     ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[0], &temperature));
    //     SYS_LOG( "temperature read from DS18B20[0]: %.2fC", temperature);
    //     // e.g., Print memory usage statistics or uptime
    //     size_t heap = esp_get_free_heap_size();
    //     if (heap >= 1024 * 1024) {
    //         SYS_LOG("System Uptime: %lu minutes | Free Heap: %.2f MB", uptime_minutes, heap / (1024.0 * 1024.0));
    //     } else if (heap >= 1024) {
    //         SYS_LOG("System Uptime: %lu minutes | Free Heap: %.2f KB", uptime_minutes, heap / 1024.0);
    //     } else {
    //         SYS_LOG("System Uptime: %lu minutes | Free Heap: %u bytes", uptime_minutes, (unsigned int)heap);
    //     }
        
    //     uptime_minutes++;
    //     vTaskDelay(pdMS_TO_TICKS(5000)); // Tick once per minute
    // }
}