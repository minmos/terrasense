#include "app_logic.h"
#include "sensors.h"
#include "hass_discovery.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"  
#include "net_mqtt.h"

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
}

void app_logic_handle_mqtt(const char *topic, const char *payload)
{
    SYS_LOG("app_logic MQTT -> Topic: %s | Payload: %s", topic, payload);
}

// ----------------------------------------------------------------------------
// Sensor data publish task
// ----------------------------------------------------------------------------
static void sensor_data_publish_task(void *pvParameters)
{
    SYS_LOG("Sensor data publish task started.");
    sensor_data_t data;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_DATA_PUBLISH_INTERVAL_MS));

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
        vTaskDelay(pdMS_TO_TICKS(SENSOR_LOG_INTERVAL_MS));

        if (!sensors_get_data(&data)) continue;

        SYS_LOG("--- ENVIRONMENT UPDATE ---");

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
    }
}

// ----------------------------------------------------------------------------
// Init
// ----------------------------------------------------------------------------
void app_logic_init(sys_debug_led_t *led)
{
    run_hass_discovery();
    xTaskCreate(sensor_log_task, "sensor_log_task", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_data_publish_task, "sensor_data_publish_task", 4096, NULL, 4, NULL);
}