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


// ----------------------------------------------------------------------------
// Discovery
// ----------------------------------------------------------------------------
static void run_hass_discovery(void)
{
    SYS_LOG("Publishing HASS discovery...");

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
            .device_id           = HARDWARE_SHT3X_CONFIG[i].mqtt_device_id, // Shared topic base
            .unique_id           = temp_uid,                                // Unique HASS identifier
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
            .device_id           = HARDWARE_SHT3X_CONFIG[i].mqtt_device_id, // Shared topic base
            .unique_id           = hum_uid,                                 // Unique HASS identifier
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
            .type                = HA_ENTITY_BINARY_SENSOR, // Assuming HA_ENTITY_BINARY_SENSOR exists in your hass_discovery.h
            .device_id           = HARDWARE_BINARY_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_BINARY_CONFIG[i].name,
            // .device_class        = HARDWARE_BINARY_CONFIG[i].device_class, // E.g. "door" // https://www.home-assistant.io/integrations/binary_sensor/
            .device_class        = "None", // currenlty 'None' is generic, if this is needed, just add a member to binary_sensor_target_t in sensor.h and adapt HARDWARE_BINARY_CONFIG in sys_config.h
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
        
        // 1. The Fan Control Entity
        ha_discovery_config_t cfg_fan = {
            .type                = HA_ENTITY_FAN,
            .device_id           = HARDWARE_FAN_CONFIG[i].mqtt_device_id,
            .name                = HARDWARE_FAN_CONFIG[i].name,
            .availability_topic  = MQTT_AVAILABILITY_TOPIC,
            .force_update        = false,
        };
        hass_discovery_publish(&cfg_fan);

        // 2. The Tachometer Sensor Entity (If Supported)
        if (HARDWARE_FAN_CONFIG[i].tach_pin != FAN_UNUSED_PIN) {
            char rpm_name[64];
            char rpm_uid[64];
            snprintf(rpm_name, sizeof(rpm_name), "%s RPM", HARDWARE_FAN_CONFIG[i].name);
            snprintf(rpm_uid, sizeof(rpm_uid), "%s_rpm", HARDWARE_FAN_CONFIG[i].mqtt_device_id);

            ha_discovery_config_t cfg_rpm = {
                .type                = HA_ENTITY_SENSOR,
                // Using the exact same device_id groups the RPM sensor beautifully under the Fan card in HA
                .device_id           = HARDWARE_FAN_CONFIG[i].mqtt_device_id, 
                .unique_id           = rpm_uid,
                .name                = rpm_name,
                .state_class         = "measurement",
                .unit_of_measurement = "RPM",
                .value_template      = "{{ value_json.rpm }}",
                .availability_topic  = MQTT_AVAILABILITY_TOPIC,
                .force_update        = false,
            };
            hass_discovery_publish(&cfg_rpm);
        }
    }
#endif
}

void app_logic_handle_mqtt(const char *topic, const char *payload)
{
    SYS_LOG("app_logic MQTT -> Topic: %s | Payload: %s", topic, payload);

#if HARDWARE_SWITCH_ENABLED
    // Check if the incoming topic matches any of our configured switches
    for (int i = 0; i < HARDWARE_SWITCH_COUNT; i++) {
        char expected_cmd_topic[128];
        snprintf(expected_cmd_topic, sizeof(expected_cmd_topic), MQTT_COMMAND_TOPIC("switch", "%s"), HARDWARE_SWITCH_CONFIG[i].mqtt_device_id);

        if (strcmp(topic, expected_cmd_topic) == 0) {
            // Determine requested state
            bool new_state = (strcmp(payload, "ON") == 0);
            
            // Set the physical hardware
            gpio_switch_set_state(i, new_state);
            
            // Immediately confirm the new state to Home Assistant so the UI toggle snaps into place
            publish_switch_state(i, new_state);
            
            SYS_LOG("MQTT Triggered Switch '%s' -> %s", HARDWARE_SWITCH_CONFIG[i].name, new_state ? "ON" : "OFF");
            return; // Command handled, exit function
        }
    }
#endif

#if HARDWARE_FAN_ENABLED
    for (int i = 0; i < HARDWARE_FAN_COUNT; i++) {
        char expected_cmd_topic[128];
        char expected_pct_topic[128];
        
        snprintf(expected_cmd_topic, sizeof(expected_cmd_topic), MQTT_COMMAND_TOPIC("fan", "%s"), HARDWARE_FAN_CONFIG[i].mqtt_device_id);
        snprintf(expected_pct_topic, sizeof(expected_pct_topic), MQTT_BASE_TOPIC "/fan/%s/percentage_command", HARDWARE_FAN_CONFIG[i].mqtt_device_id);

        // 1. Handle Power Commands (ON/OFF)
        if (strcmp(topic, expected_cmd_topic) == 0) {
            bool turn_on = (strcmp(payload, "ON") == 0);
            // If toggled ON, default to 100%. If OFF, cut to 0%.
            uint8_t new_speed = turn_on ? 100 : 0;
            
            actor_fan_set_speed(i, new_speed);
            publish_fan_state(i);
            SYS_LOG("MQTT Triggered Fan '%s' Power -> %s", HARDWARE_FAN_CONFIG[i].name, payload);
            return;
        }

        // 2. Handle Precision Speed Commands (0-100 Slider)
        if (strcmp(topic, expected_pct_topic) == 0) {
            int percent = atoi(payload);
            
            actor_fan_set_speed(i, (uint8_t)percent);
            publish_fan_state(i);
            SYS_LOG("MQTT Triggered Fan '%s' Percentage -> %d%%", HARDWARE_FAN_CONFIG[i].name, percent);
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
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", data.ds18b20_temps[i]);
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
            
            // Bundle temp and humidity in one JSON payload
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", data.sht3x_temps[i]);
            cJSON_AddNumberToObject(root, "humidity", data.sht3x_hums[i]);
            char *payload = cJSON_PrintUnformatted(root);

            char state_topic[128];
            // Publish to the shared base topic!
            snprintf(state_topic, sizeof(state_topic), MQTT_STATE_TOPIC("sensor", "%s"), HARDWARE_SHT3X_CONFIG[i].mqtt_device_id);

            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            
            free(payload);
            cJSON_Delete(root);
        }
#endif

#if HARDWARE_BINARY_ENABLED
        for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
            cJSON *root = cJSON_CreateObject();
            
            // Home Assistant binary sensors usually expect "ON" or "OFF" strings
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
            // Read whatever the state currently is (might have been changed by MQTT!)
            bool current_state = gpio_switch_get_state(i);
            
            // Invert it
            bool new_state = !current_state;
            
            // Apply it
            gpio_switch_set_state(i, new_state);
            
            // Notify Home Assistant of the background change
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


// ----------------------------------------------------------------------------
// Init
// ----------------------------------------------------------------------------
void app_logic_init(sys_debug_led_t *led)
{
    run_hass_discovery();

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

    xTaskCreate(sensor_log_task, "sensor_log_task", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_data_publish_task, "sensor_data_publish_task", 4096, NULL, 4, NULL);
    // xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 4, NULL);
#if HARDWARE_FAN_ENABLED
    xTaskCreate(fan_data_publish_task, "fan_data_publish_task", 4096, NULL, 4, NULL);
#endif
}