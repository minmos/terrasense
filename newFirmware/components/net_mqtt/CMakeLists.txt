idf_component_register(SRCS "net_mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "sys_utils" "mqtt" "esp_event")
