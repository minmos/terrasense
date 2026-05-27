idf_component_register(SRCS "actors.c" "gpio_switch.c" "actor_fan.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "sys_utils" "sys_config" "esp_driver_ledc" "esp_driver_pcnt")