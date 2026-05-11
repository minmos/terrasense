#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// System & Core Components
#include "sys_config.h"
#include "sys_utils.h"
#include "sys_led.h" 
#include "sys_ota.h" 

// Network Components
#include "net_core.h"
#include "net_mqtt.h" 

// Application Logic (Future includes)
#include "sensors.h"  // For I2C (SHT35) etc...
#include "app_logic.h"
// #include "app_relays.h"   // For GPIO (Heater, Pump, Vents)
// #include "app_control.h"  // The brain: turns on heater if temp is low

// Global Handles
sys_debug_led_t builtin_status_led;
#define GPIO_BUILTIN_LED 8


// ============================================================================
// 1. MQTT ROUTING DISPATCHER
// ============================================================================
static void mqtt_router_callback(const char *topic, const char *payload)
{
    // --- System-level commands (belong in main) ---
    if (strstr(topic, MQTT_OTA_SUBTOPIC) != NULL) {
        SYS_LOG("OTA Command Received!");
        sys_ota_start(&builtin_status_led, payload);
        return;
    }

    // --- Everything else goes to app logic ---
    app_logic_handle_mqtt(topic, payload);
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
    SYS_LOG("Initializing Hardware Peripherals...");
    // app_sensors_init();   // Sets up I2C bus and ADC pins
    // app_relays_init();    // Configures GPIO pins to output and sets them LOW
    // app_control_init();   // Loads saved target temps from NVS
    sensors_init();

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
    net_mqtt_subscribe(MQTT_OTA_SUBTOPIC, 1);

    
    // ------------------------------------------------------------------------
    // PHASE 4: Application Logic
    // ------------------------------------------------------------------------
    app_logic_init(&builtin_status_led);

    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_OK_DAY);
    SYS_LOG("Boot complete. Entering system monitor.");

    // ------------------------------------------------------------------------
    // PHASE 5: The Main Loop (System Monitor)
    // ------------------------------------------------------------------------
    // Since all heavy lifting is now handled by FreeRTOS tasks and callbacks,
    // the main loop is relegated to a slow-ticking system monitor or watchdog.

    
    // ------------------------------------------------------------------------
    // PHASE 5: System Monitor
    // ------------------------------------------------------------------------
    uint32_t uptime_minutes = 0;
    while (1) {

        size_t heap = esp_get_free_heap_size();
        if (heap >= 1024 * 1024) {
            SYS_LOG("System Uptime: %lu minutes | Free Heap: %.2f MB", uptime_minutes, heap / (1024.0 * 1024.0));
        } else if (heap >= 1024) {
            SYS_LOG("System Uptime: %lu minutes | Free Heap: %.2f KB", uptime_minutes, heap / 1024.0);
        } else {
            SYS_LOG("System Uptime: %lu minutes | Free Heap: %u bytes", uptime_minutes, (unsigned int)heap);
        }
        
        uptime_minutes++;
        vTaskDelay(pdMS_TO_TICKS(1 * MINUTE));
    }

}












// sensor_data_t current_env_data;
// while (1) {
//     // Safely fetch a copy of the latest background readings
//     if (sensors_get_data(&current_env_data)) {
//         SYS_LOG("--- ENVIRONMENT UPDATE TESTING OTA UPDATE---");
        
//         for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
//             float temp = current_env_data.ds18b20_temps[i];
            
//             if (temp != SENSOR_VALUE_INVALID) {
//                 char payload[64];
//                 snprintf(payload, sizeof(payload), "{\"temperature\": %.2f}", temp);
//                 // Publish to MQTT
//                 net_mqtt_publish_raw("terrasense/this_is_a_terrarium_id/sensor/ds18b20_temporaryplaceholder/state", payload, 0, false);

//                 SYS_LOG("%s: %.2f °C", HARDWARE_DS18B20_CONFIG[i].name, temp);
//             } else {
//                 SYS_LOG_ERR("%s: OFFLINE", HARDWARE_DS18B20_CONFIG[i].name);
//             }
//         }
//     }
    
//     vTaskDelay(pdMS_TO_TICKS(20 * SECOND));
// }

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







