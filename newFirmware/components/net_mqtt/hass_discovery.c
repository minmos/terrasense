#include "hass_discovery.h"
#include "sys_config.h"
#include "net_mqtt.h" 
#include "cJSON.h"    
#include "sys_utils.h"

bool hass_discovery_publish(const ha_discovery_config_t *config)
{
    if (config == NULL) return false;

    // 1. Determine the HA Component String 
    const char *component_str = "";
    switch (config->type) {
        case HA_ENTITY_SENSOR: component_str = "sensor"; break;
        case HA_ENTITY_SWITCH: component_str = "switch"; break;
        case HA_ENTITY_BINARY_SENSOR: component_str = "binary_sensor"; break;
        case HA_ENTITY_FAN: component_str = "fan"; break; 
        case HA_ENTITY_NUMBER: component_str = "number"; break;
        default: return false;
    }

    const char *discovery_id = config->unique_id ? config->unique_id : config->device_id;

    // 2. Generate the dynamic topics 
    char discovery_topic[256];
    char state_topic[256];
    char command_topic[256];
    
    snprintf(discovery_topic, sizeof(discovery_topic), MQTT_HASS_AUTODISCOVERY_TOPIC("%s", "%s"), component_str, discovery_id);
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("%s", "%s"), component_str, config->device_id);
    
    // Both Switches and Fans use basic ON/OFF command topics
    if (config->type == HA_ENTITY_SWITCH || config->type == HA_ENTITY_FAN || config->type == HA_ENTITY_NUMBER) {
        snprintf(command_topic, sizeof(command_topic), MQTT_COMMAND_TOPIC("%s", "%s"), component_str, config->device_id);
    }

    // 3. Build the JSON Object 
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "name", config->name);
    cJSON_AddStringToObject(root, "unique_id", discovery_id); 
    cJSON_AddStringToObject(root, "state_topic", state_topic);

    if (config->device_class) cJSON_AddStringToObject(root, "device_class", config->device_class);
    if (config->state_class) cJSON_AddStringToObject(root, "state_class", config->state_class);
    if (config->unit_of_measurement) cJSON_AddStringToObject(root, "unit_of_measurement", config->unit_of_measurement);
    if (config->icon) cJSON_AddStringToObject(root, "icon", config->icon);
    
    if (config->value_template) cJSON_AddStringToObject(root, "value_template", config->value_template);
    if (config->availability_topic) cJSON_AddStringToObject(root, "availability_topic", config->availability_topic);
    if (config->force_update) cJSON_AddTrueToObject(root, "force_update");

    if (config->type == HA_ENTITY_SWITCH || config->type == HA_ENTITY_FAN || config->type == HA_ENTITY_NUMBER) {
        cJSON_AddStringToObject(root, "command_topic", command_topic);
    }

    // --- FAN SPECIFIC CAPABILITIES ---
    if (config->type == HA_ENTITY_FAN) {
        char pct_cmd[256];
        char pct_state[256];
        snprintf(pct_cmd, sizeof(pct_cmd), MQTT_BASE_TOPIC "/fan/%s/percentage_command", config->device_id);
        snprintf(pct_state, sizeof(pct_state), MQTT_BASE_TOPIC "/fan/%s/percentage_state", config->device_id);
        
        cJSON_AddStringToObject(root, "percentage_command_topic", pct_cmd);
        cJSON_AddStringToObject(root, "percentage_state_topic", pct_state);
    }
    if (config->type == HA_ENTITY_NUMBER) {
        cJSON_AddNumberToObject(root, "min", config->min_value);
        cJSON_AddNumberToObject(root, "max", config->max_value);
        if (config->step > 0) {
            cJSON_AddNumberToObject(root, "step", config->step);
        }
        if (config->mode != NULL) {
            cJSON_AddStringToObject(root, "mode", config->mode);
        }
    }

    // --- Device Grouping ---
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "identifiers", TERRARIUM_ID);
    cJSON_AddStringToObject(device, "name", TERRARIUM_ID);
    cJSON_AddStringToObject(device, "manufacturer", "DROPOUT IOT Alliance");
    cJSON_AddStringToObject(device, "model", "ESP32-C6");
    cJSON_AddItemToObject(root, "device", device);

    // 4. Convert and Publish
    char *json_string = cJSON_PrintUnformatted(root);
    bool success = net_mqtt_publish_raw(discovery_topic, json_string, 1, true); 

    free(json_string);
    cJSON_Delete(root);

    SYS_LOG("WE did autodiscovery for device_id: %s", discovery_id);
    return success;
}