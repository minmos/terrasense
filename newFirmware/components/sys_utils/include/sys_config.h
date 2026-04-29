// components/sys_utils/include/sys_config.h
#pragma once

// --- Network & Timings ---
#define CTRL_LOOP_PERIOD_MS   2000 
#define WIFI_SSID_DEFAULT     "MyNetwork"

// --- Hardware Pins ---
#define PIN_I2C_SDA           21
#define PIN_I2C_SCL           22
#define PIN_ACTOR_HEATER      4
#define PIN_ACTOR_MISTER      5
#define PIN_SENSOR_WATER      18

// --- Application Defaults ---
#define DEFAULT_TEMP_DAY      28.0f
#define DEFAULT_TEMP_NIGHT    22.0f