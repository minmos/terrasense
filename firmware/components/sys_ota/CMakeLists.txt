idf_component_register(SRCS "sys_ota.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "sys_utils" "esp_https_ota" "esp_http_client" "app_update")
