#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sys_utils.h"
#include "sys_config.h"

#include "sensors.h"
#include "sensor_ds18b20.h"

// --- Private State ---
static sensor_data_t current_sensor_data;
static SemaphoreHandle_t sensor_mutex = NULL;

/**
 * This is a background task fetching all the sensor values that are available and storing them in current_data. 
 * current_data is protected by sensor_mutex. 
 */
static void sensor_polling_task(void *pvParameter)
{
    SYS_LOG("Sensor polling task started.");
    
    // currently only for ds18b20 sensors
#if HARDWARE_DS18B20_ENABLED
    float temp_buffer[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY];
#endif
    while (1) {
#if HARDWARE_DS18B20_ENABLED
        sensor_ds18b20_read(temp_buffer);
        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
                current_sensor_data.ds18b20_temps[i] = temp_buffer[i];
            }
            xSemaphoreGive(sensor_mutex);
        }
#endif
        // delay next sensor values fetching
        vTaskDelay(pdMS_TO_TICKS(SENSOR_FETCHING_INTERVAL));
    }
}

void sensors_init(void)
{
#if HARDWARE_DS18B20_ENABLED
    if (HARDWARE_DS18B20_COUNT > SENSOR_DS18B20_COMPONENT_MAX_CAPACITY) {
        SYS_LOG_ERR("Configured DS18B20 sensors exceed max capacity!");
        configASSERT(0);
    }
    for (int i = 0; i < SENSOR_DS18B20_COMPONENT_MAX_CAPACITY; i++) {
        current_sensor_data.ds18b20_temps[i] = SENSOR_VALUE_INVALID;
    }
    sensor_ds18b20_init(ONEWIRE_BUS_GPIO, HARDWARE_DS18B20_CONFIG, HARDWARE_DS18B20_COUNT);
#endif

    sensor_mutex = xSemaphoreCreateMutex();
    xTaskCreate(sensor_polling_task, "sensor_task", 4096, NULL, 5, NULL);
}

bool sensors_get_data(sensor_data_t *out_data)
{
    if (sensor_mutex == NULL || out_data == NULL) return false;

    // Try to lock the mutex for up to 100ms
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *out_data = current_sensor_data; // Copy the data safely
        xSemaphoreGive(sensor_mutex);
        return true;
    }
    
    return false;
}
