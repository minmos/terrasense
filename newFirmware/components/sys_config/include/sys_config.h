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


//* ********************************* */
//* ****** MQTT Configurations ****** */
//* ********************************* */
#define MQTT_DOMAIN_PREFIX  "terrasense" 
#define TERRARIUM_ID        "this_is_a_terrarium_id"
#define MQTT_BASE_TOPIC     MQTT_DOMAIN_PREFIX "/" TERRARIUM_ID

#define MQTT_HASS_AUTODISCOVERY_TOPIC(component, device_id)     "homeassistant/" component "/" MQTT_DOMAIN_PREFIX "-" TERRARIUM_ID "-" device_id "/config"
#define MQTT_STATE_TOPIC(component, device_id)                  MQTT_BASE_TOPIC "/" component "/" device_id "/state"
#define MQTT_COMMAND_TOPIC(component, device_id)                MQTT_BASE_TOPIC "/" component "/" device_id "/command"
#define MQTT_AVAILABILITY_TOPIC                                 MQTT_BASE_TOPIC "/status"

#define MQTT_OTA_SUBTOPIC   "cmd/update"


//* ********************************* */
//* ***** Sensor Configurations ***** */
//* ********************************* */
#define SENSOR_FETCHING_INTERVAL            2 * SECOND
#define SENSOR_LOG_INTERVAL_MS              45 * SECOND
#define SENSOR_DATA_PUBLISH_INTERVAL_MS     45 * SECOND

// --- Sensor Pins ---
#define ONEWIRE_BUS_GPIO 18
#define I2C_MASTER_PORT  0
#define I2C_SDA_GPIO     5 
#define I2C_SCL_GPIO     9
// #define SHT35_I2C_ADDR   SHT3X_I2C_ADDR_GND 
#define SHT35_I2C_ADDR    0x44

//* --- DS18B20 Sensor Configurations ---
// Set to 1 to enable DS18B20 sensors, 0 to disable entirely
#define HARDWARE_DS18B20_ENABLED 0
#if HARDWARE_DS18B20_ENABLED
//! ADHERE TO NAMING SCHEME FROM mqtt_namingscheme.md
static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Temporärer Temp DS18B20-Sensor", .mqtt_device_id = "ds18b20_temporaryplaceholder", .rom_address = 0x133C6CF64930E728 },
    // { .name = "xyz Temp", .rom_address = 0x... }
};
// The compiler counts how many DS18B20 sensors we have, need to know the size for creating correct sensor_data_t struct
#define HARDWARE_DS18B20_COUNT (sizeof(HARDWARE_DS18B20_CONFIG) / sizeof(HARDWARE_DS18B20_CONFIG[0]))
#else
#define HARDWARE_DS18B20_COUNT 0
#endif

//* --- SHT3X / FS304 Sensor Configurations ---
// Set to 1 to enable SHT35 sensors, 0 to disable
#define HARDWARE_SHT3X_ENABLED 1
#if HARDWARE_SHT3X_ENABLED
static const sht3x_target_t HARDWARE_SHT3X_CONFIG[] = {
    // mux_channel: -1 means it is wired directly to the I2C bus (no multiplexer)
    // Once you add the TCA9548A, change mux_channel to 0, 1, 2, etc.
    { .name = "SHT35 testsensor", .mqtt_device_id = "sht35_ambient", .i2c_address = SHT35_I2C_ADDR, .mux_channel = -1 },
    // { .name = "SHT35 Hot Side", .mqtt_device_id = "sht35_hot", .i2c_address = 0x44, .mux_channel = 0 },
};
#define HARDWARE_SHT3X_COUNT (sizeof(HARDWARE_SHT3X_CONFIG) / sizeof(HARDWARE_SHT3X_CONFIG[0]))
#else
#define HARDWARE_SHT3X_COUNT 0
#endif

// (Future pins will go here)
// #define RELAY_HEATER_GPIO 5