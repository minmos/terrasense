// components/sys_utils/include/sys_utils.h
#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include <string.h>

// --- Custom Logging Macros ---

// 1. Helper to strip the full directory path from __FILE__ so we only get "main.c" or "sensors.c"
// Renamed to SYS_FILENAME to avoid collision with ESP-IDF v6.0+ internal macros
#define SYS_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// 2. The actual log macros. These route into ESP-IDF's logger, using the filename as the TAG.
// The `##__VA_ARGS__` allows you to pass variables just like printf.
#define SYS_LOG(fmt, ...)      ESP_LOGI(SYS_FILENAME, fmt, ##__VA_ARGS__)
#define SYS_LOG_WARN(fmt, ...) ESP_LOGW(SYS_FILENAME, fmt, ##__VA_ARGS__)
#define SYS_LOG_ERR(fmt, ...)  ESP_LOGE(SYS_FILENAME, fmt, ##__VA_ARGS__)


// --- Custom Error Handling ---
#define SYS_ERR_CHECK(x, msg) do { \
    esp_err_t __err_rc = (x); \
    if (__err_rc != ESP_OK) { \
        SYS_LOG_ERR("%s: %s", msg, esp_err_to_name(__err_rc)); \
    } \
} while(0)

void func(void);