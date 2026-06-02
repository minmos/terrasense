if(CONFIG_TERRARIUM_CRAWL_CAY)
    set(APP_LOGIC_SRC "app_logic_crawl_cay.c")
else()
    set(APP_LOGIC_SRC "app_logic_test.c")
endif()

idf_component_register(SRCS ${APP_LOGIC_SRC}
                    INCLUDE_DIRS "include"
                    REQUIRES "sys_utils" "net_core" "net_mqtt" "sys_ota" "actors" "app_logic" "net_zigbee" "sensors" "sys_config" "sht3x" "i2cdev")