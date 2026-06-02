#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sys_utils.h"
#include "sys_led.h"

#define SENSOR_VALUE_INVALID (-999.0f)
#define SENSOR_DS18B20_COMPONENT_MAX_CAPACITY 8 
#define SENSOR_SHT3X_COMPONENT_MAX_CAPACITY   8
#define SENSOR_BINARY_COMPONENT_MAX_CAPACITY  8

// Structs are used by sys_config.h to define which sensor with corresponding special parts we have
typedef struct {
    const char *name;
    const char *mqtt_device_id; 
    uint64_t rom_address; 
} ds18b20_target_t;

typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int8_t mux_channel;   
} sht3x_target_t;

typedef struct {
    const char *name;
    const char *mqtt_device_id;
    int gpio_pin;
    bool invert_logic;        // if true: GND=ON. If false: 3.3V=ON.
} binary_sensor_target_t;

// a global threadsafe datastructure to keep track of all the sensor data/states we have, in app logic we only fetch this datastructure so we dont have to interact with any sensors there
// the array sizes here are macros defined by the compiler to fit the defined amount in sys_config.h
typedef struct {
    float ds18b20_temps[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY]; 
    float sht3x_temps[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
    float sht3x_hums[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
    bool  binary_states[SENSOR_BINARY_COMPONENT_MAX_CAPACITY];
} sensor_data_t;

void sensors_init(sys_debug_led_t *debug_led);
bool sensors_get_data(sensor_data_t *out_data);