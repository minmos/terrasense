#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "sys_config.h"
#include "sys_utils.h"
#include "sys_led.h" 
#include "sys_ota.h" 

#include "net_core.h"
#include "net_mqtt.h" 

#include "sensors.h"  
#include "actors.h"
#include "app_logic.h"

sys_debug_led_t builtin_status_led;

/**
 * We can only have one global mqtt callback function. Chose to reside in main. Main only handles OTA callback, 
 * rest is forwarded to app logic.
 */
static void mqtt_router_callback(const char *topic, const char *payload)
{
    if (strstr(topic, MQTT_OTA_SUBTOPIC) != NULL) {
        SYS_LOG("OTA Command Received!");
        sys_ota_start(&builtin_status_led, payload);
        return;
    }
    app_logic_handle_mqtt(topic, payload);
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500)); 
    SYS_LOG("=========================================");
    SYS_LOG("Terrasense Greenhouse Controller Booting!");
    SYS_LOG("=========================================");

    //* ----- Core System Initialization -----
    sys_led_init(&builtin_status_led); 
    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_BOOTING);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    SYS_ERR_CHECK(ret, "Failed to initialize NVS");

    //* ----- Hardware Peripherals Initialization -----
    SYS_LOG("Initializing Hardware Peripherals...");
    sensors_init(&builtin_status_led);
    actors_init(&builtin_status_led);

    //* ----- Network Initialization -----
    SYS_LOG("Initializing Networking Stack...");
    net_core_init(&builtin_status_led);
    // adding some delays to ensure we have a clean startup
    vTaskDelay(pdMS_TO_TICKS(5000));
    net_mqtt_init(&builtin_status_led, mqtt_router_callback);
    net_mqtt_start();
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    //* Subscribe to OTA command topic
    net_mqtt_subscribe(MQTT_OTA_SUBTOPIC, 1);

    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_OK_DAY);
    SYS_LOG("Boot complete. Entering system monitor.");

    //* ----- Application Logic -----
    app_logic_init(&builtin_status_led);

    //* ----- The Main Loop (System Monitor) -----
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

