#include "app_logic.h"
#include "sensors.h"
#include "hass_discovery.h"
#include "sys_config.h"
#include "sys_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"  

#include "net_mqtt.h"

// --- NEW SHT35 INCLUDES ---
#include "sht3x.h"
#include <string.h>

// --- TEMPORARY I2C PINS (Update these for your ESP32-C6) ---
#define TEST_I2C_PORT       0
#define TEST_I2C_SDA_GPIO   5 
#define TEST_I2C_SCL_GPIO   9 
// Note: SHT3X_I2C_ADDR_GND is usually 0x44. If your ADDR pin is pulled high, use SHT3X_I2C_ADDR_VDD (0x45)
#define SHT35_I2C_ADDR      SHT3X_I2C_ADDR_GND 

static sht3x_t sht35_dev;

static void test_sht35_sensor(void)
{
    SYS_LOG("Initializing I2C and SHT35 (FS304) Sensor...");

    // 1. Initialize the i2cdev library (Required by esp-idf-lib)
    ESP_ERROR_CHECK(i2cdev_init());

    // 2. Setup the sensor descriptor
    memset(&sht35_dev, 0, sizeof(sht3x_t));
    ESP_ERROR_CHECK(sht3x_init_desc(&sht35_dev, SHT35_I2C_ADDR, TEST_I2C_PORT, TEST_I2C_SDA_GPIO, TEST_I2C_SCL_GPIO));

    // 3. Initialize the sensor hardware
    esp_err_t res = sht3x_init(&sht35_dev);
    if (res != ESP_OK) {
        SYS_LOG_ERR("Could not initialize SHT35. Check wiring, pull-up resistors, and I2C address!");
        return;
    }

    // 4. Perform a test measurement
    float temperature = 0.0;
    float humidity = 0.0;
    res = sht3x_measure(&sht35_dev, &temperature, &humidity);
    
    if (res == ESP_OK) {
        SYS_LOG("SUCCESS! SHT35 Found -> Temp: %.2f °C, Hum: %.2f %%RH", temperature, humidity);
    } else {
        SYS_LOG_ERR("SHT35 Measure failed with code: %d", res);
    }
}

// ----------------------------------------------------------------------------
// Discovery
// ----------------------------------------------------------------------------
static void run_hass_discovery(void)
{
    SYS_LOG("Publishing HASS discovery...");

    //* DS18B20 sensor discover
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
#if HARDWARE_DS18B20_ENABLED
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] == SENSOR_VALUE_INVALID) {
                SYS_LOG_ERR("%s: OFFLINE, skipping.", HARDWARE_DS18B20_CONFIG[i].name);
                continue;
            }
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", data.ds18b20_temps[i]);
            char *payload = cJSON_PrintUnformatted(root);

            char state_topic[128];
            snprintf(state_topic, sizeof(state_topic),
                     MQTT_STATE_TOPIC("sensor", "%s"),
                     HARDWARE_DS18B20_CONFIG[i].mqtt_device_id);

            net_mqtt_publish_raw(state_topic, payload, 1, 0);
            SYS_LOG("Published %s: %.2f C", HARDWARE_DS18B20_CONFIG[i].name, data.ds18b20_temps[i]);

            free(payload);
            cJSON_Delete(root);
        }
#endif

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

        SYS_LOG("--- ENVIRONMENT UPDATE ---");

#if HARDWARE_DS18B20_ENABLED
        for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
            if (data.ds18b20_temps[i] != SENSOR_VALUE_INVALID) {
                SYS_LOG("%s: %.2f C", HARDWARE_DS18B20_CONFIG[i].name, data.ds18b20_temps[i]);
            } else {
                SYS_LOG_ERR("%s: OFFLINE", HARDWARE_DS18B20_CONFIG[i].name);
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
    test_sht35_sensor();
    run_hass_discovery();
    xTaskCreate(sensor_log_task, "sensor_log_task", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_data_publish_task, "sensor_data_publish_task", 4096, NULL, 4, NULL);
}