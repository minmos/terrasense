idf_component_register(SRCS "app_logic.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "sys_utils" "net_core" "net_mqtt" "sys_ota" "actors" "app_logic" "net_zigbee" "sensors" "sys_config")