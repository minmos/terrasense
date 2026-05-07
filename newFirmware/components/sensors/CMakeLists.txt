idf_component_register(
    SRCS "sensors.c" "sensor_ds18b20.c"
    INCLUDE_DIRS "include"
    REQUIRES "ds18b20" "onewire_bus" "sys_utils" "sys_config"
)