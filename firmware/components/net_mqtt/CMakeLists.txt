idf_component_register(
    SRCS "hass_discovery.c" "net_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES "sys_utils" "sys_config" "mqtt" "esp_event" "sensors" "cjson"
)