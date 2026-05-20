idf_component_register(SRCS "sys_utils.c" "sys_led.c"
                    INCLUDE_DIRS "include" 
                    REQUIRES "led_strip" "sys_config")
