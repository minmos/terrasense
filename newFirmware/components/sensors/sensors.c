#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sys_utils.h"

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

// --- Public API ---
void sensors_init(int onewire_pin, const ds18b20_target_t *ds18b20_targets, int ds18b20_count)
{
    // Safety check: Don't overflow our internal memory cap
    if (ds18b20_count > SENSOR_COMPONENT_MAX_CAPACITY) {
        SYS_LOG_ERR("WARNING: Configured sensors (%d) exceeds max capacity (%d)! Capping.", 
                    ds18b20_count, SENSOR_COMPONENT_MAX_CAPACITY);
        ds18b20_count = SENSOR_COMPONENT_MAX_CAPACITY;
    }

    current_data.ds18b20_count = ds18b20_count;

    // Fill the initial state with INVALID values
    for (int i = 0; i < SENSOR_COMPONENT_MAX_CAPACITY; i++) {
        current_data.ds18b20_temps[i] = SENSOR_VALUE_INVALID;
    }

    // Create the Mutex lock
    sensor_mutex = xSemaphoreCreateMutex();
    if (sensor_mutex == NULL) {
        SYS_LOG_ERR("Failed to create sensor mutex!");
        return;
    }

    // Pass the Read-Only targets straight down to the worker
    sensor_ds18b20_init(onewire_pin, ds18b20_targets, current_data.ds18b20_count);

    // Spawn the background polling task on Core 0 (since C6 is single-core)
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
