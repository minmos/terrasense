#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SENSOR_VALUE_INVALID (-999.0f)

// A generous memory cap for the background struct (8 sensors = 32 bytes of RAM)
#define SENSOR_COMPONENT_MAX_CAPACITY 8

// Struct used by hardware_pin_config.h
typedef struct {
    const char *name;
    uint64_t rom_address;
} ds18b20_target_t;

// The global thread-safe data structure
typedef struct {
    float ds18b20_temps[SENSOR_COMPONENT_MAX_CAPACITY];
    int ds18b20_count; // How many the user ACTUALLY configured
} sensor_data_t;

// API
void sensors_init(int onewire_pin, const ds18b20_target_t *ds18b20_targets, int ds18b20_count);
bool sensors_get_data(sensor_data_t *out_data);