#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sys_utils.h"
#include "sys_config.h"

#include "sensors.h"
#include "sensor_ds18b20.h"

// --- Private State ---
static sensor_data_t current_data;
static SemaphoreHandle_t sensor_mutex = NULL;

/**
 * This is a background task fetching all the sensor values that are available and storing them in current_data. 
 * current_data is protected by sensor_mutex. 
 */
static void sensor_polling_task(void *pvParameter)
{
    SYS_LOG("Sensor polling task started.");
    
    // Local buffer to hold readings before we lock the mutex
    float temp_buffer[SENSOR_COMPONENT_MAX_CAPACITY];

    while (1) {
        // 1. Tell the worker to read the hardware (this blocks for ~800ms)
        sensor_ds18b20_read(temp_buffer);

        // 2. Lock the Mutex to update the global public state
        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) {
            
            for(int i = 0; i < current_data.ds18b20_count; i++) {
                current_data.ds18b20_temps[i] = temp_buffer[i];
            }
            
            xSemaphoreGive(sensor_mutex);
        }

        // Wait 4 seconds before reading again
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void sensors_init(void)
{
    // 2. Read directly from the config macros
    int configured_count = HARDWARE_DS18B20_COUNT;

    if (configured_count > SENSOR_COMPONENT_MAX_CAPACITY) {
        SYS_LOG_ERR("WARNING: Configured sensors exceed max capacity!");
        configured_count = SENSOR_COMPONENT_MAX_CAPACITY;
    }

    current_data.ds18b20_count = configured_count;

    for (int i = 0; i < SENSOR_COMPONENT_MAX_CAPACITY; i++) {
        current_data.ds18b20_temps[i] = SENSOR_VALUE_INVALID;
    }

    sensor_mutex = xSemaphoreCreateMutex();

    // 3. Pass the config macros down to the worker
    sensor_ds18b20_init(ONEWIRE_BUS_GPIO, HARDWARE_DS18B20_CONFIG, current_data.ds18b20_count);

    xTaskCreate(sensor_polling_task, "sensor_task", 4096, NULL, 5, NULL);
}

bool sensors_get_data(sensor_data_t *out_data)
{
    if (sensor_mutex == NULL || out_data == NULL) return false;

    // Try to lock the mutex for up to 100ms
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *out_data = current_data; // Copy the data safely
        xSemaphoreGive(sensor_mutex);
        return true;
    }
    
    return false;
}





// --- Home Assistant Auto-Discovery Routine ---
/* static void publish_ha_discovery(void)
{
    SYS_LOG("Publishing Home Assistant Auto-Discovery Data...");

    // The topic must be: homeassistant/<component>/<node_id>/<object_id>/config
    const char *discovery_topic = "homeassistant/sensor/terracotta/temperature/config";

    // Buffer needs to be large enough to hold the JSON (512 bytes is safe here)
    char payload[512];
    
    // Build the JSON string. 
    // Notice the "dev" block: this groups all your sensors under one "Terracotta" device in HA!
    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"Greenhouse Temperature\","
        "\"state_topic\":\"terracotta/sensor/temp\","
        "\"value_template\":\"{{ value_json.temp }}\","
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\","
        "\"unique_id\":\"terracotta_temp_1\","
        "\"device\":{"
            "\"identifiers\":[\"terracotta_c6_01\"],"
            "\"name\":\"Terracotta Controller\","
            "\"manufacturer\":\"Theo\""
        "}"
        "}"
    );
    // snprintf(payload, sizeof(payload),
    //     "{"
    //     "\"name\":\"Greenhouse Temperature\","
    //     "\"stat_t\":\"terracotta/sensor/temp\","
    //     "\"val_tpl\":\"{{ value_json.temp }}\","
    //     "\"unit_of_meas\":\"°C\","
    //     "\"dev_cla\":\"temperature\","
    //     "\"dev\":{"
    //         "\"identifiers\":[\"terracotta_c6_01\"],"
    //         "\"name\":\"Terracotta Controller\","
    //         "\"manufacturer\":\"Theo\""
    //     "}"
    //     "}"
    // );

    // Publish using our new raw function. QoS 1, Retain 1 (Crucial!)
    ESP_ERROR_CHECK(net_mqtt_publish_raw(discovery_topic, payload, 1, 1));
} */
