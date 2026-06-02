// components/sys_config/include/sys_config.h
#pragma once
#include "sensors.h"
#include "actors.h"

//* ********************************* */
//* ***** General Configurations **** */
//* ********************************* */
// using ms sleep function pdMS_TO_TICKS()
#define SECOND          1000
#define MINUTE          60 * SECOND
#define HOUR            60 * MINUTE

//* --- Debug Configs ---
#define SYS_LED_DEBUG_MODE_ENABLED  1



//* ********************************* */
//* ****** MQTT Configurations ****** */
//* ********************************* */
#define MQTT_DOMAIN_PREFIX  "terrasense" 
#define TERRARIUM_ID        "crawl_cay_terrarium"
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
#define SENSOR_LOG_INTERVAL                 10 * SECOND
#define SENSOR_DATA_PUBLISH_INTERVAL        10 * SECOND
#define CONTROL_LOOP_INTERVAL               SECOND

//* --- Sensor Pins ---
#define ONEWIRE_BUS_GPIO    4
#define I2C_MASTER_PORT     0 
#define I2C_SDA_GPIO        5 
#define I2C_SCL_GPIO        6
#define SHT35_I2C_ADDR      0x44
#define MUX_I2C_ADDRESS     0x70
#define BINARY_SENSOR_GPIO  11


//* --- DS18B20 Sensor Configurations ---
// Set to 1 to enable DS18B20 sensors, 0 to disable entirely
#define HARDWARE_DS18B20_ENABLED 1
#if HARDWARE_DS18B20_ENABLED
//! ADHERE TO NAMING SCHEME FROM mqtt_namingscheme.md

static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Far Left", .mqtt_device_id = "far_left", .rom_address = 0x6B08225467CF0328 },
    { .name = "Ground", .mqtt_device_id = "ground", .rom_address = 0x48082254936BE028 },
    { .name = "Far Right", .mqtt_device_id = "far_right", .rom_address = 0xD707226171338E28 },
    { .name = "Right Platform", .mqtt_device_id = "right_platform", .rom_address = 0x930822547953C728 },
    { .name = "Left Platform", .mqtt_device_id = "left_platform", .rom_address = 0x4A0922537AB94C28 },
    { .name = "Heatlamp", .mqtt_device_id = "heatlamp", .rom_address = 0xF93C88F649B8A828 }
};

// The compiler counts how many DS18B20 sensors we have, need to know the size for creating correct sensor_data_t struct
#define HARDWARE_DS18B20_COUNT (sizeof(HARDWARE_DS18B20_CONFIG) / sizeof(HARDWARE_DS18B20_CONFIG[0]))
#else
#define HARDWARE_DS18B20_COUNT 0
#endif

#define HEATLAMP_DS18B20_NAME "Heatlamp"
#define RIGHT_PLATFORM_DS18B20_NAME "Right Platform"
#define MAXIMUM_HEATLAMP_TEMP 43

//* --- SHT3X / FS304 Sensor Configurations ---
// Set to 1 to enable SHT35 sensors, 0 to disable
#define HARDWARE_SHT3X_ENABLED 1
#if HARDWARE_SHT3X_ENABLED
static const sht3x_target_t HARDWARE_SHT3X_CONFIG[] = {
    { .name = "Center", .mqtt_device_id = "center", .mux_channel = 0 },
    // { .name = "SHT35 Hot Side", .mqtt_device_id = "sht35_hot", .mux_channel = 1 },
};
#define HARDWARE_SHT3X_COUNT (sizeof(HARDWARE_SHT3X_CONFIG) / sizeof(HARDWARE_SHT3X_CONFIG[0]))
#else
#define HARDWARE_SHT3X_COUNT 0
#endif

//* --- Binary Sensor Configurations ---
// Set to 1 to enable Binary sensors, 0 to disable
#define HARDWARE_BINARY_ENABLED 1
#if HARDWARE_BINARY_ENABLED
static const binary_sensor_target_t HARDWARE_BINARY_CONFIG[] = {
    { .name = "Tank Status", .mqtt_device_id = "liquid_level_sensor", .gpio_pin = BINARY_SENSOR_GPIO, .invert_logic = true },
};
#define HARDWARE_BINARY_COUNT (sizeof(HARDWARE_BINARY_CONFIG) / sizeof(HARDWARE_BINARY_CONFIG[0]))
#else
#define HARDWARE_BINARY_COUNT 0
#endif


//* ********************************* */
//* ***** Actuator Configurations *** */
//* ********************************* */

//* --- SSR Control Pins ---

#define RELAY_0_GPIO  10
#define RELAY_1_GPIO  1
#define RELAY_2_GPIO  0
#define SSR_RELAY_GPIO  7

//* --- GPIO Switch ((solid-state)Relay) Configurations ---
#define HARDWARE_SWITCH_ENABLED 1
#if HARDWARE_SWITCH_ENABLED
static const switch_target_t HARDWARE_SWITCH_CONFIG[] = {
    { .name = "Lights", .mqtt_device_id = "lights", .gpio_pin = RELAY_0_GPIO, .active_high = true, .default_state = false },
    { .name = "Misting System", .mqtt_device_id = "misting_system", .gpio_pin = RELAY_1_GPIO, .active_high = true, .default_state = false },
    // { .name = "TEST_RELAY2", .mqtt_device_id = "test_relay2", .gpio_pin = RELAY_2_GPIO, .active_high = true, .default_state = false },
    { .name = "Ceramic Heater", .mqtt_device_id = "ceramic_heater", .gpio_pin = SSR_RELAY_GPIO, .active_high = true, .default_state = false },
};
#define HARDWARE_SWITCH_COUNT (sizeof(HARDWARE_SWITCH_CONFIG) / sizeof(HARDWARE_SWITCH_CONFIG[0]))
#else
#define HARDWARE_SWITCH_COUNT 0
#endif


//* --- Fan Configurations ---
#define RPM_UPDATE_INTERVAL 2 * SECOND
#define FAN_UNUSED_PIN      -1
#define FAN0_POWER_PIN      18
#define FAN0_TACH_PIN       19
#define FAN0_PWN_PIN        20

#define HARDWARE_FAN_ENABLED 1
#if HARDWARE_FAN_ENABLED
static const fan_target_t HARDWARE_FAN_CONFIG[] = {
    { .name = "Fan", .mqtt_device_id = "fan", .power_pin = FAN0_POWER_PIN, .tach_pin = FAN0_TACH_PIN, .pwm_pin = FAN0_PWN_PIN, .is_4pin = false },
    // { .name = "neuer drei pin Fan", .mqtt_device_id = "fan_dreinewnewnew", .power_pin = FAN0_POWER_PIN, .tach_pin = FAN_UNUSED_PIN, .pwm_pin = FAN_UNUSED_PIN, .is_4pin = false },
};
#define HARDWARE_FAN_COUNT (sizeof(HARDWARE_FAN_CONFIG) / sizeof(HARDWARE_FAN_CONFIG[0]))
#else
#define HARDWARE_FAN_COUNT 0
#endif

//* --- Number Configurations ---
typedef struct {
    const char *name;
    const char *mqtt_device_id;
    float min_val;
    float max_val;
    float step;
    float default_val;
    const char *mode;
} number_target_t;

#define HARDWARE_NUMBER_ENABLED 1
#if HARDWARE_NUMBER_ENABLED
static const number_target_t HARDWARE_NUMBER_CONFIG[] = {
    { 
        .name = "Target Temperature Day", 
        .mqtt_device_id = "target_temp_day", 
        .min_val = 20.0, 
        .max_val = 32.0, 
        .step = 0.5, 
        .default_val = 28.0, 
        .mode = "slider"
    },
    { 
        .name = "Target Temperature Night", 
        .mqtt_device_id = "target_temp_night", 
        .min_val = 20.0, 
        .max_val = 32.0, 
        .step = 0.5, 
        .default_val = 22.0, 
        .mode = "slider"
    },
    { 
        .name = "Sunset Mist Time Seconds", 
        .mqtt_device_id = "sunset_mist_time_seconds", 
        .min_val = 10.0, 
        .max_val = 300.0, 
        .step = 5, 
        .default_val = 60.0, 
        .mode = "box"
    },
    { 
        .name = "Sunrise Fan Time Minutes", 
        .mqtt_device_id = "sunrise_fan_time_minutes", 
        .min_val = 10.0, 
        .max_val = 300.0, 
        .step = 5, 
        .default_val = 120.0, 
        .mode = "box"
    },
    { 
        .name = "Sunrise Fan Speed", 
        .mqtt_device_id = "sunrise_fan_speed", 
        .min_val = 0.0, 
        .max_val = 100.0, 
        .step = 5, 
        .default_val = 60.0, 
        .mode = "box"
    },
};
#define HARDWARE_NUMBER_COUNT (sizeof(HARDWARE_NUMBER_CONFIG) / sizeof(HARDWARE_NUMBER_CONFIG[0]))
#else
#define HARDWARE_NUMBER_COUNT 0
#endif