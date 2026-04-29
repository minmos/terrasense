#include <stdio.h>
#include "actors.h"

void func(void)
{

}

// --- Home Assistant Auto-Discovery Routine ---
/* static void publish_ha_discovery(void)
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
} */
