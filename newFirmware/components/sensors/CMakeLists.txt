idf_component_register(
    SRCS "sensor_binary.c" "sensors.c" "sensor_ds18b20.c" "sensor_sht3x.c" "sensor_binary.c"
    INCLUDE_DIRS "include"
    REQUIRES "ds18b20" "onewire_bus" "sys_utils" "sys_config" "net_mqtt" "sht3x" "i2cdev" "tca9548" "driver"
)