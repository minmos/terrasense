#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_VALUE_INVALID (-999.0f)
#define SENSOR_DS18B20_COMPONENT_MAX_CAPACITY 8 // A generous memory cap for the background struct (8 sensors = 32 bytes of RAM)
#define SENSOR_SHT3X_COMPONENT_MAX_CAPACITY   8
#define SENSOR_BINARY_COMPONENT_MAX_CAPACITY  8

// Struct used by sys_config.h to define which sensor and corresponding address we have
typedef struct {
    const char *name;
    const char *mqtt_device_id; 
    uint64_t rom_address; 
} ds18b20_target_t;

// SHT3x target config
typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int8_t mux_channel;   // -1 for direct connection, 0-7 for TCA9548A I2C Multiplexer
} sht3x_target_t;

typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int gpio_pin;
    bool invert_logic;        // If true: GND = ON (Recommended for switches). If false: 3.3V = ON.
} binary_sensor_target_t;

// The global thread-safe data structure
typedef struct {
    float ds18b20_temps[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY]; // the amount of ds18b20 sensor is defined by compiler in /components/sys_config/include/sys_config.h
    float sht3x_temps[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
    float sht3x_hums[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
    bool  binary_states[SENSOR_BINARY_COMPONENT_MAX_CAPACITY];
} sensor_data_t;

// API
void sensors_init();
bool sensors_get_data(sensor_data_t *out_data);