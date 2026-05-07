// main/hardware_pin_config.h
#pragma once
#include "sensors.h"



//* ********************************* */
//* ***** Sensor Configurations ***** */
//* ********************************* */
// --- Sensor Pins ---
#define ONEWIRE_BUS_GPIO 18
// --- DS18B20 Sensor Configuration ---
static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Temporärer Temp DS18B20-Sensor",  .rom_address = 0x133C6CF64930E728 },
    // { .name = "xyz Temp", .rom_address = 0x... }
};
// The compiler counts how many DS18B20 sensors we have, need to know the size for creating correct sensor_data_t struct
#define HARDWARE_DS18B20_COUNT (sizeof(HARDWARE_DS18B20_CONFIG) / sizeof(HARDWARE_DS18B20_CONFIG[0]))



// (Future pins will go here)
// #define I2C_SDA_GPIO 21
// #define RELAY_HEATER_GPIO 5