if(CONFIG_TERRARIUM_CRAWL_CAY)
    set(SYS_CONFIG_INC "include/crawl_cay")
else()
    set(SYS_CONFIG_INC "include/test")
endif()

idf_component_register(INCLUDE_DIRS ${SYS_CONFIG_INC}
                    REQUIRES "sensors" "actors"
                    )