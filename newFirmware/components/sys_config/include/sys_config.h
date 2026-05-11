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
#define SENSOR_LOG_INTERVAL_MS              5 * SECOND
#define SENSOR_DATA_PUBLISH_INTERVAL_MS     5 * SECOND

// --- Sensor Pins ---
#define ONEWIRE_BUS_GPIO 18

//* --- DS18B20 Sensor Configuration ---
//! ADHERE TO NAMING SCHEME FROM mqtt_namingscheme.md
static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Temporärer Temp DS18B20-Sensor", .mqtt_device_id = "ds18b20_temporaryplaceholder", .rom_address = 0x133C6CF64930E728 },
    // { .name = "xyz Temp", .rom_address = 0x... }
};
// The compiler counts how many DS18B20 sensors we have, need to know the size for creating correct sensor_data_t struct
#define HARDWARE_DS18B20_COUNT (sizeof(HARDWARE_DS18B20_CONFIG) / sizeof(HARDWARE_DS18B20_CONFIG[0]))



// (Future pins will go here)
// #define I2C_SDA_GPIO 21
// #define RELAY_HEATER_GPIO 5