// components/sys_config/include/sys_config.h
#pragma once
#include "sensors.h"

//* ********************************* */
//* ***** General Configurations **** */
//* ********************************* */
// using ms sleep function pdMS_TO_TICKS()
#define SECOND          1000
#define MINUTE          60 * SECOND
#define HOUR            60 * MINUTE
// HomeAssistant MQTT AutoDiscovery topic, HASS_MQTT_AUTODISCOVERY_TOPIC("sensor", "special-type") would expand to homeassistant/sensor/terracotta/special-type/config
#define HASS_MQTT_AUTODISCOVERY_TOPIC(component, type) "homeassistant/" component "/" CONFIG_MQTT_BASE_TOPIC "/" type "/config"


//* ********************************* */
//* ***** Sensor Configurations ***** */
//* ********************************* */
#define SENSOR_FETCHING_INTERVAL 10 * SECOND

// --- Sensor Pins ---
#define ONEWIRE_BUS_GPIO 18

//* --- DS18B20 Sensor Configuration ---
static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Temporärer Temp DS18B20-Sensor", .mqtt_suffix = "special_sensor_type", .rom_address = 0x133C6CF64930E728 },
    // { .name = "xyz Temp", .rom_address = 0x... }
};
// The compiler counts how many DS18B20 sensors we have, need to know the size for creating correct sensor_data_t struct
#define HARDWARE_DS18B20_COUNT (sizeof(HARDWARE_DS18B20_CONFIG) / sizeof(HARDWARE_DS18B20_CONFIG[0]))



// (Future pins will go here)
// #define I2C_SDA_GPIO 21
// #define RELAY_HEATER_GPIO 5