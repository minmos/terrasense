#pragma once
#include <stdbool.h>

// used as the type for MQTT autodiscovery messages
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
    
    const char *value_template;      
    const char *availability_topic;  
    bool force_update;               

    float min_value;
    float max_value;
    float step;
    const char *mode; 
} ha_discovery_config_t;

bool hass_discovery_publish(const ha_discovery_config_t *config);