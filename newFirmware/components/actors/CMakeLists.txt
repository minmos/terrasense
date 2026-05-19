idf_component_register(SRCS "actors.c" "gpio_switch.c"
                        INCLUDE_DIRS "include"
                        REQUIRES "sys_utils" "sys_config" "driver")
