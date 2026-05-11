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

    //* DS18B20 sensor discover
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

    // Future sensor types go here, same pattern:
    // for (int i = 0; i < HARDWARE_SHT_COUNT; i++) { ... }
}


void app_logic_handle_mqtt(const char *topic, const char *payload)
{
    SYS_LOG("app_logic MQTT -> Topic: %s | Payload: %s", topic, payload);

    // Future actuator commands go here:
    // if (strstr(topic, "cmd/heater")     != NULL) { ... }
    // if (strstr(topic, "cmd/pump")       != NULL) { ... }
    // if (strstr(topic, "cmd/target_temp")!= NULL) { ... }
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
            SYS_LOG_WARN("MQTT not connected, skipping telemetry.");
            continue;
        }

        if (!sensors_get_data(&data)) {
            SYS_LOG_WARN("Failed to acquire sensor data.");
            continue;
        }

        // --- DS18B20 temperatures ---
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] == SENSOR_VALUE_INVALID) {
                SYS_LOG_ERR("%s: OFFLINE, skipping publish.", HARDWARE_DS18B20_CONFIG[i].name);
                continue;
            }

            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", data.ds18b20_temps[i]);
            char *payload = cJSON_PrintUnformatted(root);

            // Expands to e.g: "terrasense/this_is_a_terrarium_id/sensor/ds18b20_temporaryplaceholder/state"
            char state_topic[128];
            snprintf(state_topic, sizeof(state_topic),
                     MQTT_STATE_TOPIC("sensor", "%s"),
                     HARDWARE_DS18B20_CONFIG[i].mqtt_device_id);

            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            SYS_LOG("Published %s: %.2f C", HARDWARE_DS18B20_CONFIG[i].name, data.ds18b20_temps[i]);

            free(payload);
            cJSON_Delete(root);
        }

        // Future sensor types go here, same pattern:
        // for (int i = 0; i < HARDWARE_SHT_COUNT; i++) { ... }
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

        if (!sensors_get_data(&data)) {
            SYS_LOG_WARN("Failed to acquire sensor data.");
            continue;
        }

        SYS_LOG("--- ENVIRONMENT OTA UPDATE ---");
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] != SENSOR_VALUE_INVALID) {
                SYS_LOG("%s: %.2f C", HARDWARE_DS18B20_CONFIG[i].name, data.ds18b20_temps[i]);
            } else {
                SYS_LOG_ERR("%s: OFFLINE", HARDWARE_DS18B20_CONFIG[i].name);
            }
        }
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