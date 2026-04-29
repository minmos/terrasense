#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "sys_utils.h"
#include "sys_config.h"
#include "net_core.h"
#include "sys_led.h" 
#include "net_mqtt.h" // Add the new include

// This is the builtin LED on ESP32-C6
sys_debug_led_t builtin_status_led;


// The callback function that gets triggered when a message arrives
static void on_mqtt_message_received(const char *topic, const char *payload)
{
    SYS_LOG("INCOMING MQTT -> Topic: %s | Payload: %s", topic, payload);
    
    // Later, you will route this to your app_logic!
    // e.g., if (strcmp(topic, "terracotta/cmd/heater") == 0) { ... }
}

// --- Home Assistant Auto-Discovery Routine ---
static void publish_ha_discovery(void)
{
    SYS_LOG("Publishing Home Assistant Auto-Discovery Data...");

    // The topic must be: homeassistant/<component>/<node_id>/<object_id>/config
    const char *discovery_topic = "homeassistant/sensor/terracotta/temperature/config";

    // Buffer needs to be large enough to hold the JSON (512 bytes is safe here)
    char payload[512];
    
    // Build the JSON string. 
    // Notice the "dev" block: this groups all your sensors under one "Terracotta" device in HA!
    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"Greenhouse Temperature\","
        "\"state_topic\":\"terracotta/sensor/temp\","
        "\"value_template\":\"{{ value_json.temp }}\","
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\","
        "\"unique_id\":\"terracotta_temp_1\","
        "\"device\":{"
            "\"identifiers\":[\"terracotta_c6_01\"],"
            "\"name\":\"Terracotta Controller\","
            "\"manufacturer\":\"Theo\""
        "}"
        "}"
    );
    // snprintf(payload, sizeof(payload),
    //     "{"
    //     "\"name\":\"Greenhouse Temperature\","
    //     "\"stat_t\":\"terracotta/sensor/temp\","
    //     "\"val_tpl\":\"{{ value_json.temp }}\","
    //     "\"unit_of_meas\":\"°C\","
    //     "\"dev_cla\":\"temperature\","
    //     "\"dev\":{"
    //         "\"identifiers\":[\"terracotta_c6_01\"],"
    //         "\"name\":\"Terracotta Controller\","
    //         "\"manufacturer\":\"Theo\""
    //     "}"
    //     "}"
    // );

    // Publish using our new raw function. QoS 1, Retain 1 (Crucial!)
    ESP_ERROR_CHECK(net_mqtt_publish_raw(discovery_topic, payload, 1, 1));
}


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    
    sys_led_init(&builtin_status_led, 8, 1); 
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

    net_core_init(&builtin_status_led);

    vTaskDelay(pdMS_TO_TICKS(5000));

    SYS_LOG("Boot complete. Entering DAY mode.");
    sys_led_set_state(&builtin_status_led, SYS_LED_STATE_OK_DAY);
    
    // 1. Initialize MQTT, give it the LED and the Callback
    SYS_LOG("Init of mqtt");
    net_mqtt_init(&builtin_status_led, on_mqtt_message_received);

    net_mqtt_start();
    SYS_LOG("now after pause we publish auto discovery mqtt message");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Give it a moment to connect
    
    publish_ha_discovery();
    SYS_LOG("mqtt auto discovery message was sent");
    // 4. Subscribe to a command topic using your clever Subtopic system
    net_mqtt_subscribe("cmd/target_temp", 1); 

    int loop_counter = 0;
    while (1) {
        // Publish a fake sensor reading every 5 seconds
        if ((loop_counter % 3) == 0)
        {
            char payload[32];
            snprintf(payload, sizeof(payload), "{\"temp\": 24.%d}", loop_counter);
            // This will publish to: "terracotta/sensor/temp"
            net_mqtt_publish("sensor/temp", payload, 1, 0); 
        }

        loop_counter++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}