#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sys_utils.h"
#include "sys_config.h"

#include "sensors.h"
#include "sensor_ds18b20.h"
#include "sensor_sht3x.h"
#include "sensor_binary.h"
// Required for the underlying I2C library shared by sensors
#include "i2cdev.h"

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
    
#if HARDWARE_DS18B20_ENABLED
    float ds18b20_temp_buffer[SENSOR_DS18B20_COMPONENT_MAX_CAPACITY];
#endif
#if HARDWARE_SHT3X_ENABLED
    float sht3x_temp_buffer[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
    float sht3x_hum_buffer[SENSOR_SHT3X_COMPONENT_MAX_CAPACITY];
#endif
#if HARDWARE_BINARY_ENABLED
    bool binary_buffer[SENSOR_BINARY_COMPONENT_MAX_CAPACITY];
#endif

    while (1) {
#if HARDWARE_DS18B20_ENABLED
        sensor_ds18b20_read(ds18b20_temp_buffer);
#endif
#if HARDWARE_SHT3X_ENABLED
        sensor_sht3x_read(sht3x_temp_buffer, sht3x_hum_buffer);
#endif
#if HARDWARE_BINARY_ENABLED
        sensor_binary_read(binary_buffer);
#endif

        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) {
#if HARDWARE_DS18B20_ENABLED
            for (int i = 0; i < HARDWARE_DS18B20_COUNT; i++) {
                current_sensor_data.ds18b20_temps[i] = ds18b20_temp_buffer[i];
            }
#endif
#if HARDWARE_SHT3X_ENABLED
            for (int i = 0; i < HARDWARE_SHT3X_COUNT; i++) {
                current_sensor_data.sht3x_temps[i] = sht3x_temp_buffer[i];
                current_sensor_data.sht3x_hums[i]  = sht3x_hum_buffer[i];
            }
#endif
#if HARDWARE_BINARY_ENABLED
            for (int i = 0; i < HARDWARE_BINARY_COUNT; i++) {
                current_sensor_data.binary_states[i] = binary_buffer[i];
            }
#endif
            xSemaphoreGive(sensor_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_FETCHING_INTERVAL));
    }
}

void sensors_init(void)
{
    // Initialize the shared I2C device library first
    ESP_ERROR_CHECK(i2cdev_init());
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
#if HARDWARE_SHT3X_ENABLED
    if (HARDWARE_SHT3X_COUNT > SENSOR_SHT3X_COMPONENT_MAX_CAPACITY) {
        SYS_LOG_ERR("Configured SHT3X sensors exceed max capacity!");
        configASSERT(0);
    }
    for (int i = 0; i < SENSOR_SHT3X_COMPONENT_MAX_CAPACITY; i++) {
        current_sensor_data.sht3x_temps[i] = SENSOR_VALUE_INVALID;
        current_sensor_data.sht3x_hums[i]  = SENSOR_VALUE_INVALID;
    }
    sensor_sht3x_init(I2C_MASTER_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO, HARDWARE_SHT3X_CONFIG, HARDWARE_SHT3X_COUNT);
#endif
#if HARDWARE_BINARY_ENABLED
    if (HARDWARE_BINARY_COUNT > SENSOR_BINARY_COMPONENT_MAX_CAPACITY) {
        SYS_LOG_ERR("Configured Binary sensors exceed max capacity!");
        configASSERT(0);
    }
    for (int i = 0; i < SENSOR_BINARY_COMPONENT_MAX_CAPACITY; i++) {
        current_sensor_data.binary_states[i] = false;
    }
    sensor_binary_init();
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
