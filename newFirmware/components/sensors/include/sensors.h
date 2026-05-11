#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_VALUE_INVALID (-999.0f)
#define SENSOR_DS18B20_COMPONENT_MAX_CAPACITY 8 // A generous memory cap for the background struct (8 sensors = 32 bytes of RAM)

// Struct used by sys_config.h to define which sensor and corresponding address we have
typedef struct {
    const char *name;
    const char *mqtt_suffix; // used for mqtt auto discovery
    uint64_t rom_address; 
} ds18b20_target_t;

// The global thread-safe data structure
typedef struct {
    float ds18b20_temps[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY]; // the amount of ds18b20 sensor is defined by compiler in /components/sys_config/include/sys_config.h
} sensor_data_t;

// API
void sensors_init();
bool sensors_get_data(sensor_data_t *out_data);