#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// System & Core Components
#include "sys_utils.h"
#include "sys_config.h"
#include "sys_led.h" 
#include "sys_ota.h" 

// Network Components
#include "net_core.h"
#include "net_mqtt.h" 

// Application Logic (Future includes)
// #include "app_sensors.h"  // For I2C (BME280) and ADC (Soil Moisture)
// #include "app_relays.h"   // For GPIO (Heater, Pump, Vents)
// #include "app_control.h"  // The brain: turns on heater if temp is low

// Global Handles
sys_debug_led_t builtin_status_led;
#define GPIO_BUILTIN_LED 8

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
    uint32_t uptime_minutes = 0;
    while (1) {
        // e.g., Print memory usage statistics or uptime
        SYS_LOG("System Uptime: %lu minutes | Free Heap: %lu bytes", uptime_minutes, esp_get_free_heap_size());
        
        uptime_minutes++;
        vTaskDelay(pdMS_TO_TICKS(60000)); // Tick once per minute
    }
}