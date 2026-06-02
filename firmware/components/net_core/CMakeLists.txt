idf_component_register(
    SRCS "net_core.c"
    INCLUDE_DIRS "include"
    REQUIRES "sys_utils" "esp_wifi" "nvs_flash" "lwip" "esp_netif"
)