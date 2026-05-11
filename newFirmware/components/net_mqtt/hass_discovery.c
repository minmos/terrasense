#include "hass_discovery.h"
#include "sys_config.h"
#include "net_mqtt.h" // Your MQTT publishing function
#include "cJSON.h"    // ESP-IDF built-in JSON library
#include "sys_utils.h"

bool hass_discovery_publish(const ha_discovery_config_t *config)
{
    if (config == NULL) return false;

    // 1. Determine the HA Component String (used for the topic)
    const char *component_str = "";
    switch (config->type) {
        case HA_ENTITY_SENSOR: component_str = "sensor"; break;
        case HA_ENTITY_SWITCH: component_str = "switch"; break;
        default: return false;
    }

    // 2. Generate the dynamic topics using your awesome sys_config macros
    char discovery_topic[256];
    char state_topic[256];
    char command_topic[256];
    
    snprintf(discovery_topic, sizeof(discovery_topic), MQTT_HASS_AUTODISCOVERY_TOPIC("%s", "%s"), component_str, config->device_id);
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("%s", "%s"), component_str, config->device_id);
    
    // Only generate command topic if it's a switch
    if (config->type == HA_ENTITY_SWITCH) {
        snprintf(command_topic, sizeof(command_topic), MQTT_COMMAND_TOPIC("%s", "%s"), component_str, config->device_id);
    }

    // 3. Build the JSON Object safely using cJSON
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "name", config->name);
    cJSON_AddStringToObject(root, "unique_id", config->device_id);
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    if (config->device_class) cJSON_AddStringToObject(root, "device_class", config->device_class);
    if (config->state_class) cJSON_AddStringToObject(root, "state_class", config->state_class);
    if (config->unit_of_measurement) cJSON_AddStringToObject(root, "unit_of_measurement", config->unit_of_measurement);
    if (config->icon) cJSON_AddStringToObject(root, "icon", config->icon);
    
    if (config->value_template) cJSON_AddStringToObject(root, "value_template", config->value_template);
    if (config->availability_topic) cJSON_AddStringToObject(root, "availability_topic", config->availability_topic);
    if (config->force_update) cJSON_AddTrueToObject(root, "force_update");

    if (config->type == HA_ENTITY_SWITCH) {
        cJSON_AddStringToObject(root, "command_topic", command_topic);
    }

    // --- Device Grouping (Optional but makes HA look awesome) ---
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "identifiers", TERRARIUM_ID);
    cJSON_AddStringToObject(device, "name", "TerraSense Controller");
    cJSON_AddStringToObject(device, "manufacturer", "Theo's Custom Hardware");
    cJSON_AddStringToObject(device, "model", "ESP32-C6");
    cJSON_AddItemToObject(root, "device", device);

    // 4. Convert JSON object to a raw string
    char *json_string = cJSON_PrintUnformatted(root);
    
    // 5. Publish via your net_mqtt component!
    // Assuming you have a function like: bool net_mqtt_publish(const char* topic, const char* payload, int qos, bool retain);
    bool success = net_mqtt_publish_raw(discovery_topic, json_string, 1, true); // True = Retain message!

    // 6. Cleanup Memory (CRITICAL in C!)
    free(json_string);
    cJSON_Delete(root);

    SYS_LOG("WE did autodiscovery for device_id: %s", config->device_id);
    return success;
}