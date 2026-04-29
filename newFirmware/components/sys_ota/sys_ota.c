#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "sys_ota.h"
#include "sys_utils.h"

// Struct to pass arguments into the FreeRTOS task
typedef struct {
    sys_debug_led_t *led;
    char url[256];
} ota_task_args_t;

// The actual background download task
static void ota_background_task(void *pvParameter)
{
    ota_task_args_t *args = (ota_task_args_t *)pvParameter;
    
    SYS_LOG("Starting OTA Update from: %s", args->url);
    if (args->led) sys_led_set_state(args->led, SYS_LED_STATE_BOOTING);

    esp_http_client_config_t http_config = {
        .url = args->url,
        // Force the client to use plain HTTP over TCP, ignoring any SSL attempts
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    SYS_LOG("Downloading firmware... please wait. This may take 30-60 seconds.");
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        SYS_LOG("OTA Update Successful! Rebooting in 3 seconds...");
        if (args->led) sys_led_notify(args->led, LED_COLOR_GREEN, 10);
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart(); 
    } else {
        SYS_LOG_ERR("OTA Update Failed! Reverting to normal operation.");
        if (args->led) sys_led_set_state(args->led, SYS_LED_STATE_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (args->led) sys_led_set_state(args->led, SYS_LED_STATE_OK_DAY);
    }

    // Free the memory and kill this task
    free(args);
    vTaskDelete(NULL);
}

// The non-blocking public function
void sys_ota_start(sys_debug_led_t *led_obj, const char *url)
{
    // Allocate memory for the task arguments so they survive after this function returns
    ota_task_args_t *args = malloc(sizeof(ota_task_args_t));
    if (args == NULL) {
        SYS_LOG_ERR("Failed to allocate memory for OTA task");
        return;
    }

    args->led = led_obj;
    // Safely copy the URL, ensuring we don't overflow our 256-byte buffer
    snprintf(args->url, sizeof(args->url), "%s", url);

    // Spawn the task (8KB stack size is generally safe for OTA processes)
    xTaskCreate(&ota_background_task, "ota_task", 8192, args, 5, NULL);
}