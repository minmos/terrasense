idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES "sys_utils" "net_core" "net_mqtt" "sys_ota" "actors" "app_logic" "net_zigbee" "sensors" "sys_config")
