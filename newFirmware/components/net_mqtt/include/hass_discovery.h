#pragma once
#include <stdbool.h>

typedef enum {
    HA_ENTITY_SENSOR,
    HA_ENTITY_SWITCH,
    HA_ENTITY_BINARY_SENSOR,
    HA_ENTITY_FAN,
    HA_ENTITY_NUMBER
} ha_entity_type_t;

typedef struct {
    ha_entity_type_t type;
    const char *device_id;
    const char *unique_id;
    const char *name;
    const char *device_class;
    const char *state_class;
    const char *unit_of_measurement;
    const char *icon;
    
    const char *value_template;      // e.g., "{{ value_json.temperature }}"
    const char *availability_topic;  // e.g., "terrasense/status"
    bool force_update;               // Sends update events even if value hasn't changed

    float min_value;
    float max_value;
    float step;
    const char *mode; // e.g., "auto", "box", or "slider"
} ha_discovery_config_t;

bool hass_discovery_publish(const ha_discovery_config_t *config);