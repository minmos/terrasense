#include "app_logic.h"
#include "sensors.h"
#include "hass_discovery.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"  
#include "net_mqtt.h"
#include <string.h>
#include "actors.h"
#include "gpio_switch.h"
#include "actor_fan.h"
#include <stdlib.h>
#include "nvs.h"
#include <time.h>

static sys_debug_led_t *builtin_status_led = NULL;


static bool last_is_day = false;

// --- Sunrise/Sunset Settings ---
static uint8_t sunrise_hour = 6;
static uint8_t sunrise_minute = 0;
static uint8_t sunset_hour = 18;
static uint8_t sunset_minute = 0;

static void load_sun_times_from_nvs(void)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("sun_times", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        nvs_get_u8(my_handle, "sunrise_h", &sunrise_hour);
        nvs_get_u8(my_handle, "sunrise_m", &sunrise_minute);
        nvs_get_u8(my_handle, "sunset_h", &sunset_hour);
        nvs_get_u8(my_handle, "sunset_m", &sunset_minute);
        nvs_close(my_handle);
        SYS_LOG("Loaded sun times from NVS: Sunrise %02d:%02d, Sunset %02d:%02d", 
                sunrise_hour, sunrise_minute, sunset_hour, sunset_minute);
    } else {
        SYS_LOG("No sun times found in NVS, using default: Sunrise %02d:%02d, Sunset %02d:%02d", 
                sunrise_hour, sunrise_minute, sunset_hour, sunset_minute);
    }
}

static void save_sun_times_to_nvs(void)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("sun_times", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_u8(my_handle, "sunrise_h", sunrise_hour);
        nvs_set_u8(my_handle, "sunrise_m", sunrise_minute);
        nvs_set_u8(my_handle, "sunset_h", sunset_hour);
        nvs_set_u8(my_handle, "sunset_m", sunset_minute);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        SYS_LOG("Saved sun times to NVS: Sunrise %02d:%02d, Sunset %02d:%02d", 
                sunrise_hour, sunrise_minute, sunset_hour, sunset_minute);
    } else {
        SYS_LOG_ERR("Failed to open NVS to save sun times");
    }
}

static bool check_is_day(){
     //get day/night timings
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int current_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int sunrise_minutes = sunrise_hour * 60 + sunrise_minute;
    int sunset_minutes = sunset_hour * 60 + sunset_minute;

    if (sunrise_minutes < sunset_minutes) {
        return (current_minutes >= sunrise_minutes && current_minutes < sunset_minutes);
    } else {
        // Sunset is earlier in the day than sunrise (e.g. nocturnal custom settings or over-midnight window)
        return (current_minutes >= sunrise_minutes || current_minutes < sunset_minutes);
    }
}


// Helper to publish the current physical state of a switch back to Home Assistant
static void publish_switch_state(int index, bool state)
{
#if HARDWARE_SWITCH_ENABLED
    if (!net_mqtt_is_connected()) {
            return; 
    }
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("switch", "%s"), HARDWARE_SWITCH_CONFIG[index].mqtt_device_id);
    SYS_LOG("sending mqtt message with topic: %s with payload: %s", state_topic, state ? "ON" : "OFF");
    // Home Assistant switches default to expecting raw "ON" or "OFF" strings
    net_mqtt_publish_raw(state_topic, state ? "ON" : "OFF", 1, 1);
#endif
}

static void publish_fan_state(int index)
{
#if HARDWARE_FAN_ENABLED
    if (!net_mqtt_is_connected()) return;

    uint8_t speed = actor_fan_get_speed(index);
    const char *dev_id = HARDWARE_FAN_CONFIG[index].mqtt_device_id;

    // 1. Publish ON/OFF power state
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("fan", "%s"), dev_id);
    net_mqtt_publish_raw(state_topic, speed > 0 ? "ON" : "OFF", 1, 1);

    // 2. Publish numerical percentage state
    char pct_topic[128];
    snprintf(pct_topic, sizeof(pct_topic), MQTT_BASE_TOPIC "/fan/%s/percentage_state", dev_id);
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%d", speed);
    net_mqtt_publish_raw(pct_topic, pct_str, 1, 1);
#endif
}

#if HARDWARE_NUMBER_ENABLED
static float current_number_states[HARDWARE_NUMBER_COUNT] = {0};
#endif

// 2. Publish Helper
static void publish_number_state(int index, float value)
{
#if HARDWARE_NUMBER_ENABLED
    if (!net_mqtt_is_connected()) return; 

    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("number", "%s"), HARDWARE_NUMBER_CONFIG[index].mqtt_device_id);
    
    char payload[32];
    snprintf(payload, sizeof(payload), "%.2f", value); // Format as float with 2 decimals
    
    net_mqtt_publish_raw(state_topic, payload, 1, 1);
#endif
}

static float get_number_value_by_id(const char *mqtt_device_id, float fallback)
{
#if HARDWARE_NUMBER_ENABLED
    for (int i = 0; i < HARDWARE_NUMBER_COUNT; i++) {
        if (strcmp(HARDWARE_NUMBER_CONFIG[i].mqtt_device_id, mqtt_device_id) == 0) {
            return current_number_states[i];
        }
    }
#endif
    return fallback;
}

/**
 * Here we define our mqtt autodiscovery, if we want to change something to that, we also have to change components/net_mqtt/hass_discovery.c
 */
static void run_hass_discovery(void)
{
    SYS_LOG("Publishing HASS discovery...");

    ha_discovery_config_t cfg_sys_error = {
        .type                = HA_ENTITY_SENSOR,
        .device_id           = "sys_error_msg",
        .name                = "System Error Message",
        .icon                = "mdi:alert-circle",
        .availability_topic  = MQTT_AVAILABILITY_TOPIC,
        .force_update        = false,
    };
    hass_discovery_publish(&cfg_sys_error);

#if HARDWARE_DS18B20_ENABLED
    SYS_LOG("DS18B20 sensors HASS discovery...");
    for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
        ha_discovery_config_t cfg = {
            .type                = HA_ENTITY_SENSOR,
            .device_id           = HARDWARE_DS18B20_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_DS18B20_CONFIG[i].name,
            .device_class        = "temperature",
            .state_class         = "measurement",
            .unit_of_measurement = "°C",
            .value_template      = "{{ value_json.temperature }}",
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg);
    }
#endif

#if HARDWARE_SHT3X_ENABLED
    SYS_LOG("SHT3X sensors HASS discovery...");
    for (int i = 0; i < HARDWARE_SHT3X_COUNT; i++) {
        
        // --- Temperature Entity ---
        char temp_name[64];
        char temp_uid[64];
        snprintf(temp_name, sizeof(temp_name), "%s Temp", HARDWARE_SHT3X_CONFIG[i].name);
        snprintf(temp_uid, sizeof(temp_uid), "%s_temperature", HARDWARE_SHT3X_CONFIG[i].mqtt_device_id);
        
        ha_discovery_config_t cfg_temp = {
            .type                = HA_ENTITY_SENSOR,
            .device_id           = HARDWARE_SHT3X_CONFIG[i].mqtt_device_id, 
            .unique_id           = temp_uid,                                
            .name                = temp_name,
            .device_class        = "temperature",
            .state_class         = "measurement",
            .unit_of_measurement = "°C",
            .value_template      = "{{ value_json.temperature }}",
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_temp);

        // --- Humidity Entity ---
        char hum_name[64];
        char hum_uid[64];
        snprintf(hum_name, sizeof(hum_name), "%s Humidity", HARDWARE_SHT3X_CONFIG[i].name);
        snprintf(hum_uid, sizeof(hum_uid), "%s_humidity", HARDWARE_SHT3X_CONFIG[i].mqtt_device_id);
        
        ha_discovery_config_t cfg_hum = {
            .type                = HA_ENTITY_SENSOR,
            .device_id           = HARDWARE_SHT3X_CONFIG[i].mqtt_device_id, 
            .unique_id           = hum_uid,                                 
            .name                = hum_name,
            .device_class        = "humidity",
            .state_class         = "measurement",
            .unit_of_measurement = "%",
            .value_template      = "{{ value_json.humidity }}",
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_hum);
    }
#endif

#if HARDWARE_BINARY_ENABLED
    SYS_LOG("Binary sensors HASS discovery...");
    for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
        ha_discovery_config_t cfg_binary = {
            .type                = HA_ENTITY_BINARY_SENSOR, 
            .device_id           = HARDWARE_BINARY_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_BINARY_CONFIG[i].name,
            // .device_class        = HARDWARE_BINARY_CONFIG[i].device_class, // E.g. "door" // https://www.home-assistant.io/integrations/binary_sensor/
            .device_class        = "moisture", // currenlty 'None' is generic, if this is needed, just add a member to binary_sensor_target_t in sensor.h and adapt HARDWARE_BINARY_CONFIG in sys_config.h
            .value_template      = "{{ value_json.state }}",
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_binary);
    }
#endif

#if HARDWARE_SWITCH_ENABLED
    SYS_LOG("Switches HASS discovery...");
    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        ha_discovery_config_t cfg_switch = {
            .type                = HA_ENTITY_SWITCH,
            .device_id           = HARDWARE_SWITCH_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_SWITCH_CONFIG[i].name,
            // Switches don't need device_class or value_template for basic ON/OFF functionality
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_switch);
    }
#endif

#if HARDWARE_FAN_ENABLED
    SYS_LOG("Fans HASS discovery...");
    for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
        
        ha_discovery_config_t cfg_fan = {
            .type                = HA_ENTITY_FAN,
            .device_id           = HARDWARE_FAN_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_FAN_CONFIG[i].name,
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_fan);

        // tachometer functionality only for 4 pin fans
        if (HARDWARE_FAN_CONFIG[i].tach_pin != FAN_UNUSED_PIN) {
            char rpm_name[64];
            char rpm_uid[64];
            snprintf(rpm_name, sizeof(rpm_name), "%s RPM", HARDWARE_FAN_CONFIG[i].name);
            snprintf(rpm_uid, sizeof(rpm_uid), "%s_rpm", HARDWARE_FAN_CONFIG[i].mqtt_device_id);

            ha_discovery_config_t cfg_rpm = {
                .type                = HA_ENTITY_SENSOR,
                .device_id           = HARDWARE_FAN_CONFIG[i].mqtt_device_id, 
                .unique_id           = rpm_uid,
                .name                = rpm_name,
                .state_class         = "measurement",
                .unit_of_measurement = "RPM",
                .value_template      = "{{ value_json.rpm }}",
                .availability_topic  = MQTT_AVAILABILITY_TOPIC,
                .force_update        = false,
                .icon                = "mdi:fan-clock",
            };
            hass_discovery_publish(&cfg_rpm);
        }
    }
#endif

#if HARDWARE_NUMBER_ENABLED
    SYS_LOG("Numbers HASS discovery...");
    for (int i = 0; i < HARDWARE_NUMBER_COUNT; i++) {
        ha_discovery_config_t cfg_num = {
            .type                = HA_ENTITY_NUMBER,
            .device_id           = HARDWARE_NUMBER_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_NUMBER_CONFIG[i].name,
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .min_value           = HARDWARE_NUMBER_CONFIG[i].min_val,
            .max_value           = HARDWARE_NUMBER_CONFIG[i].max_val,
            .step                = HARDWARE_NUMBER_CONFIG[i].step,
            .mode                = HARDWARE_NUMBER_CONFIG[i].mode, 
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_num);
    }
#endif
}

void app_logic_handle_mqtt(const char *topic, const char *payload)
{
    SYS_LOG("app_logic MQTT -> Topic: %s | Payload: %s", topic, payload);

    // Check for sunrise/sunset commands
    char expected_sunrise_topic[128];
    char expected_sunset_topic[128];
    snprintf(expected_sunrise_topic, sizeof(expected_sunrise_topic), "%s/%s", MQTT_BASE_TOPIC, "cmd/sunrise");
    snprintf(expected_sunset_topic, sizeof(expected_sunset_topic), "%s/%s", MQTT_BASE_TOPIC, "cmd/sunset");

    if (strcmp(topic, expected_sunrise_topic) == 0) {
        int h = 0, m = 0;
        if (sscanf(payload, "%d:%d", &h, &m) == 2) {
            if (h >= 0 && h < 24 && m >= 0 && m < 60) {
                sunrise_hour = h;
                sunrise_minute = m;
                save_sun_times_to_nvs();
                SYS_LOG("Updated sunrise time: %02d:%02d", sunrise_hour, sunrise_minute);
            } else {
                SYS_LOG_ERR("Invalid sunrise payload values: %s", payload);
            }
        } else {
            SYS_LOG_ERR("Invalid sunrise payload format: %s", payload);
        }
        return;
    }

    if (strcmp(topic, expected_sunset_topic) == 0) {
        int h = 0, m = 0;
        if (sscanf(payload, "%d:%d", &h, &m) == 2) {
            if (h >= 0 && h < 24 && m >= 0 && m < 60) {
                sunset_hour = h;
                sunset_minute = m;
                save_sun_times_to_nvs();
                SYS_LOG("Updated sunset time: %02d:%02d", sunset_hour, sunset_minute);
            } else {
                SYS_LOG_ERR("Invalid sunset payload values: %s", payload);
            }
        } else {
            SYS_LOG_ERR("Invalid sunset payload format: %s", payload);
        }
        return;
    }

#if HARDWARE_SWITCH_ENABLED
    // callbacks for switches 
    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        char expected_cmd_topic[128];
        snprintf(expected_cmd_topic, sizeof(expected_cmd_topic), MQTT_COMMAND_TOPIC("switch", "%s"), HARDWARE_SWITCH_CONFIG[i].mqtt_device_id);

        if (strcmp(topic, expected_cmd_topic) == 0) {
            bool new_state = (strcmp(payload, "ON") == 0);
            gpio_switch_set_state(i, new_state);
            publish_switch_state(i, new_state);
            
            SYS_LOG("MQTT Triggered Switch '%s' -> %s", HARDWARE_SWITCH_CONFIG[i].name, new_state ? "ON" : "OFF");
            return; 
        }
    }
#endif

#if HARDWARE_FAN_ENABLED
    for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
        char expected_cmd_topic[128];
        char expected_pct_topic[128];
        
        snprintf(expected_cmd_topic, sizeof(expected_cmd_topic), MQTT_COMMAND_TOPIC("fan", "%s"), HARDWARE_FAN_CONFIG[i].mqtt_device_id);
        snprintf(expected_pct_topic, sizeof(expected_pct_topic), MQTT_BASE_TOPIC "/fan/%s/percentage_command", HARDWARE_FAN_CONFIG[i].mqtt_device_id);

        if (strcmp(topic, expected_cmd_topic) == 0) { //mapped speed logic for fans is implemented in actors component
            bool turn_on = (strcmp(payload, "ON") == 0);
            uint8_t new_speed = turn_on ? 100 : 0;
            
            actor_fan_set_speed(i, new_speed);
            publish_fan_state(i);
            SYS_LOG("MQTT Triggered Fan '%s' Power -> %s", HARDWARE_FAN_CONFIG[i].name, payload);
            return;
        }
        if (strcmp(topic, expected_pct_topic) == 0) {
            int percent = atoi(payload);
            
            actor_fan_set_speed(i, (uint8_t)percent);
            publish_fan_state(i);
            SYS_LOG("MQTT Triggered Fan '%s' Percentage -> %d%%", HARDWARE_FAN_CONFIG[i].name, percent);
            return;
        }
    }
#endif

#if HARDWARE_NUMBER_ENABLED
    for (int i = 0; i < HARDWARE_NUMBER_COUNT; i++) {
        char expected_cmd_topic[128];
        snprintf(expected_cmd_topic, sizeof(expected_cmd_topic), MQTT_COMMAND_TOPIC("number", "%s"), HARDWARE_NUMBER_CONFIG[i].mqtt_device_id);

        if (strcmp(topic, expected_cmd_topic) == 0) {
            float new_val = atof(payload);
            current_number_states[i] = new_val;
            publish_number_state(i, new_val);
            SYS_LOG("MQTT Triggered Number '%s' -> %.2f", HARDWARE_NUMBER_CONFIG[i].name, new_val);
            return; 
        }
    }
#endif
}

// ----------------------------------------------------------------------------
// Sensor data publish task
// ----------------------------------------------------------------------------
static void sensor_data_publish_task(void *pvParameters)
{
    SYS_LOG("Sensor data publish task started.");
    sensor_data_t data;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_DATA_PUBLISH_INTERVAL));

        if (!net_mqtt_is_connected()) {
            continue;
        }

        if (!sensors_get_data(&data)) {
            continue;
        }

#if HARDWARE_DS18B20_ENABLED
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] == SENSOR_VALUE_INVALID) continue;
            
            char temp_str[16];
            snprintf(temp_str, sizeof(temp_str), "%.1f", data.ds18b20_temps[i]);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddRawToObject(root, "temperature", temp_str);
            char *payload = cJSON_PrintUnformatted(root);

            char state_topic[128];
            snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("sensor", "%s"), HARDWARE_DS18B20_CONFIG[i].mqtt_device_id);

            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            
            free(payload);
            cJSON_Delete(root);
        }
#endif

#if HARDWARE_SHT3X_ENABLED
        for (int i = 0; i < HARDWARE_SHT3X_COUNT; i++) {
            if (data.sht3x_temps[i] == SENSOR_VALUE_INVALID || data.sht3x_hums[i] == SENSOR_VALUE_INVALID) continue;
            
            char temp_str[16];
            char hum_str[16];
            snprintf(temp_str, sizeof(temp_str), "%.1f", data.sht3x_temps[i]);
            snprintf(hum_str, sizeof(hum_str), "%.1f", data.sht3x_hums[i]);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddRawToObject(root, "temperature", temp_str);
            cJSON_AddRawToObject(root, "humidity", hum_str);
            char *payload = cJSON_PrintUnformatted(root);

            char state_topic[128];
            snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("sensor", "%s"), HARDWARE_SHT3X_CONFIG[i].mqtt_device_id);

            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            
            free(payload);
            cJSON_Delete(root);
        }
#endif

#if HARDWARE_BINARY_ENABLED
        for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
            cJSON *root = cJSON_CreateObject();
            
            cJSON_AddStringToObject(root, "state", data.binary_states[i] ? "ON" : "OFF");
            char *payload = cJSON_PrintUnformatted(root);
            char state_topic[128];
            snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("binary_sensor", "%s"), HARDWARE_BINARY_CONFIG[i].mqtt_device_id);
            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            
            free(payload);
            cJSON_Delete(root);
        }
#endif
    }
}

// ----------------------------------------------------------------------------
// Sensor log task
// ----------------------------------------------------------------------------
static void sensor_log_task(void *pvParameters)
{
    SYS_LOG("Sensor log task started.");
    sensor_data_t data;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_LOG_INTERVAL));

        if (!sensors_get_data(&data)) continue;

        // SYS_LOG("--- ENVIRONMENT UPDATE ---");

#if HARDWARE_DS18B20_ENABLED
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] != SENSOR_VALUE_INVALID) {
                SYS_LOG("[DS18B20] %s: %.2f C", HARDWARE_DS18B20_CONFIG[i].name, data.ds18b20_temps[i]);
            } else {
                SYS_LOG_ERR("[DS18B20] %s: OFFLINE", HARDWARE_DS18B20_CONFIG[i].name);
            }
        }
#endif

#if HARDWARE_SHT3X_ENABLED
        for (int i = 0; i < HARDWARE_SHT3X_COUNT; i++) {
            if (data.sht3x_temps[i] != SENSOR_VALUE_INVALID) {
                SYS_LOG("[SHT35] %s: Temp: %.2f C | Hum: %.2f %%", HARDWARE_SHT3X_CONFIG[i].name, data.sht3x_temps[i], data.sht3x_hums[i]);
            } else {
                SYS_LOG_ERR("[SHT35] %s: OFFLINE", HARDWARE_SHT3X_CONFIG[i].name);
            }
        }
#endif

#if HARDWARE_BINARY_ENABLED
        for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
            SYS_LOG("[BINARY] %s: %s", HARDWARE_BINARY_CONFIG[i].name, data.binary_states[i] ? "ON (Closed)" : "OFF (Open)");
        }
#endif
    }
}


static void gpio_task(void *pvParameters)
{
    SYS_LOG("gpio_task task started. Background toggling active.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_LOG_INTERVAL));

#if HARDWARE_SWITCH_ENABLED
        for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
            bool current_state = gpio_switch_get_state(i);
            bool new_state = !current_state;
            gpio_switch_set_state(i, new_state);
            publish_switch_state(i, new_state);
            SYS_LOG("Background Loop toggled '%s': %s", HARDWARE_SWITCH_CONFIG[i].name, new_state ? "ON" : "OFF");
        }
#endif
    }
}



#if HARDWARE_FAN_ENABLED
static void fan_data_publish_task(void *pvParameters)
{
    SYS_LOG("Fan RPM data publish task started. (2000ms Interval)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(RPM_UPDATE_INTERVAL));

        if (!net_mqtt_is_connected()) continue;

        for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
            if (HARDWARE_FAN_CONFIG[i].tach_pin != FAN_UNUSED_PIN) {
                uint32_t current_rpm = actor_fan_get_rpm(i);
                
                cJSON *root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "rpm", current_rpm);
                char *payload = cJSON_PrintUnformatted(root);

                char state_topic[128];
                snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("sensor", "%s"), HARDWARE_FAN_CONFIG[i].mqtt_device_id);

                net_mqtt_publish_raw(state_topic, payload, 1, 0);
                
                free(payload);
                cJSON_Delete(root);
            }
        }
    }
}
#endif


static void publish_system_error(const char* msg)
{
    if (!net_mqtt_is_connected()) return;
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("sensor", "sys_error_msg"));
    net_mqtt_publish_raw(state_topic, msg, 1, 1);
}

static void control_task(void *pvParameters)
{
    SYS_LOG("control_task started. Background automation active.");
    sensor_data_t sensor_data;

    // publish_system_error("Sample Test Error: Starting Control Task!");

    // Cache the hardware indices once at startup
    int heatlamp_ds18b20 = -1;
    int right_platform_ds18b20 = -1;
    for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
        if (strcmp(HARDWARE_DS18B20_CONFIG[i].name, HEATLAMP_DS18B20_NAME) == 0) {
            heatlamp_ds18b20 = i;
        }
        if (strcmp(HARDWARE_DS18B20_CONFIG[i].name, RIGHT_PLATFORM_DS18B20_NAME) == 0) {
            right_platform_ds18b20 = i;
        }
    }
    
    if (heatlamp_ds18b20 == -1) {
        SYS_LOG_ERR("Automation error: Could not find DS18B20 sensor with name '%s'", HEATLAMP_DS18B20_NAME);
    }

    if (right_platform_ds18b20 == -1) {
        SYS_LOG_ERR("Automation error: Could not find DS18B20 sensor with name '%s'", RIGHT_PLATFORM_DS18B20_NAME);
    }

    int heater_switch_idx = gpio_switch_get_index_by_id("ceramic_heater");
    int lights_switch_idx = gpio_switch_get_index_by_id("lights");
    int mister_switch_idx = gpio_switch_get_index_by_id("misting_system");

    if (heatlamp_ds18b20 == -1 || right_platform_ds18b20 == -1 || 
        heater_switch_idx == -1 || lights_switch_idx == -1 || mister_switch_idx == -1) {
        const char *err_msg = "Automation error: something failed to set up, either a relay or a ds18b20 - this is a sys_config error";
        SYS_LOG_ERR("%s", err_msg);
        publish_system_error(err_msg);
        sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    static time_t fan_off_time = 0;
    static time_t mister_off_time = 0;


    
    
    last_is_day = check_is_day();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_INTERVAL));
        
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        
        bool is_day = check_is_day();

        if (is_day){
            sys_led_set_state(builtin_status_led, SYS_LED_STATE_OK_DAY);
        }else{
            sys_led_set_state(builtin_status_led, SYS_LED_STATE_OK_NIGHT);
        }
       
        // --- 1. TEMPERATURE AUTOMATION (Heater) ---
        if (sensors_get_data(&sensor_data)) {
            float current_heatlamp_temp = sensor_data.ds18b20_temps[heatlamp_ds18b20];
            float current_right_platform_temp = sensor_data.ds18b20_temps[right_platform_ds18b20];
            if (current_right_platform_temp != SENSOR_VALUE_INVALID) {
                float current_target_temp = get_number_value_by_id(is_day ? "target_temp_day" : "target_temp_night", 25.0f);
                bool heater_should_be_on = (current_right_platform_temp < current_target_temp);
                
                // force off if heatlamp gets too hot (this will happen often, i have a shitty terrarium design )
                if (current_heatlamp_temp != SENSOR_VALUE_INVALID && current_heatlamp_temp > MAXIMUM_HEATLAMP_TEMP) {
                    heater_should_be_on = false;
                }
                
                bool current_heater_state = gpio_switch_get_state(heater_switch_idx);
                if (current_heater_state != heater_should_be_on) { 
                    gpio_switch_set_state(heater_switch_idx, heater_should_be_on);
                    publish_switch_state(heater_switch_idx, heater_should_be_on);
                    SYS_LOG("Automation: Heater turned %s (Temp: %.2fC, Target: %.2fC)", 
                            heater_should_be_on ? "ON" : "OFF", current_right_platform_temp, current_target_temp);
                }
            }
        }
        // SYS_LOG("ITS %s", is_day ? "DAY" : "NIGHT");
        

        //turn on/off the light
        bool current_light_state = gpio_switch_get_state(lights_switch_idx);
        if (current_light_state != is_day) {
            gpio_switch_set_state(lights_switch_idx, is_day);
            publish_switch_state(lights_switch_idx, is_day);
            SYS_LOG("Automation: Day/Night transition. Lights turned %s (Current: %02d:%02d, Sunrise: %02d:%02d, Sunset: %02d:%02d)",
                    is_day ? "ON" : "OFF", timeinfo.tm_hour, timeinfo.tm_min, sunrise_hour, sunrise_minute, sunset_hour, sunset_minute);
        }

        //sunrise
        if (is_day == true && last_is_day == false){
            SYS_LOG("Automation: It's sunrise at: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
#if HARDWARE_FAN_ENABLED
            float fan_time_minutes = get_number_value_by_id("sunrise_fan_time_minutes", 120.0f);
            float fan_speed_pct = get_number_value_by_id("sunrise_fan_speed", 60.0f);
            fan_off_time = now + (time_t)(fan_time_minutes * 60.0f);
            actor_fan_set_speed(0, (uint8_t)fan_speed_pct);
            publish_fan_state(0);
            SYS_LOG("Automation: Sunrise triggered. Fan set to %.0f%% for %.0f minutes.", fan_speed_pct, fan_time_minutes);
#endif
        }

        //sunset
        if (is_day == false && last_is_day == true){
            SYS_LOG("Automation: It's sunset at: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            float mist_time_seconds = get_number_value_by_id("sunset_mist_time_seconds", 120.0f);
            mister_off_time = now + (time_t)mist_time_seconds;
            gpio_switch_set_state(mister_switch_idx, true);
            publish_switch_state(mister_switch_idx, true);
            SYS_LOG("Automation: Sunset triggered. Misting system started for %.0f seconds.", mist_time_seconds);
        }

#if HARDWARE_FAN_ENABLED
        if (fan_off_time != 0 && now >= fan_off_time) {
            actor_fan_set_speed(0, 0);
            publish_fan_state(0);
            fan_off_time = 0;
            SYS_LOG("Automation: Fan turned OFF after sunrise run duration");
        }
#endif

        if (mister_off_time != 0 && now >= mister_off_time) {
            gpio_switch_set_state(mister_switch_idx, false);
            publish_switch_state(mister_switch_idx, false);
            mister_off_time = 0;
            SYS_LOG("Automation: Mister turned OFF after sunset spray duration");
        }

        last_is_day = is_day;
    }
} 



// ----------------------------------------------------------------------------
// Init
// ----------------------------------------------------------------------------
void app_logic_init(sys_debug_led_t *led)
{

    builtin_status_led = led;


    load_sun_times_from_nvs();
    run_hass_discovery();
    net_mqtt_subscribe("cmd/sunrise", 1);
    net_mqtt_subscribe("cmd/sunset", 1);

#if HARDWARE_SWITCH_ENABLED //*maybe at startup publish default state again to HA
    // We must subscribe to the command topics, otherwise the broker won't send us the messages!
    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        char cmd_subtopic[128];
        
        // Build ONLY the subtopic. Do NOT use the MQTT_COMMAND_TOPIC macro here.
        snprintf(cmd_subtopic, sizeof(cmd_subtopic), "switch/%s/command", HARDWARE_SWITCH_CONFIG[i].mqtt_device_id);
        
        SYS_LOG("subscribing to command subtopic: %s", cmd_subtopic);
        
        // Let your existing function attach the base topic naturally
        net_mqtt_subscribe(cmd_subtopic, 1);
    }
#endif
#if HARDWARE_FAN_ENABLED
    // Subscribe to both command variations for every fan
    for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
        char cmd_subtopic[128];
        char pct_subtopic[128];
        snprintf(cmd_subtopic, sizeof(cmd_subtopic), "fan/%s/command", HARDWARE_FAN_CONFIG[i].mqtt_device_id);
        snprintf(pct_subtopic, sizeof(pct_subtopic), "fan/%s/percentage_command", HARDWARE_FAN_CONFIG[i].mqtt_device_id);
        
        SYS_LOG("Subscribing to fan subtopics: %s, %s", cmd_subtopic, pct_subtopic);
        net_mqtt_subscribe(cmd_subtopic, 1);
        net_mqtt_subscribe(pct_subtopic, 1);
    }
#endif
#if HARDWARE_NUMBER_ENABLED
    for (int i = 0; i < HARDWARE_NUMBER_COUNT; i++) {
        // Load default value into memory and publish it at boot
        current_number_states[i] = HARDWARE_NUMBER_CONFIG[i].default_val;
        publish_number_state(i, current_number_states[i]);

        char cmd_subtopic[128];
        snprintf(cmd_subtopic, sizeof(cmd_subtopic), "number/%s/command", HARDWARE_NUMBER_CONFIG[i].mqtt_device_id);
        net_mqtt_subscribe(cmd_subtopic, 1);
    }
#endif

    xTaskCreate(sensor_log_task, "sensor_log_task", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_data_publish_task, "sensor_data_publish_task", 4096, NULL, 4, NULL);
    xTaskCreate(control_task, "control_task", 4096, NULL, 4, NULL);
#if HARDWARE_FAN_ENABLED
    xTaskCreate(fan_data_publish_task, "fan_data_publish_task", 4096, NULL, 4, NULL);
#endif
}