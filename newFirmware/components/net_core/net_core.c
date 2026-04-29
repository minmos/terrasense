#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "lwip/netdb.h" // For DNS resolution (Watchdog)

#include "net_core.h"
#include "sys_utils.h"
#include "sys_config.h" 
#include "sys_led.h"

static sys_debug_led_t *builtin_status_led = NULL;

// --- 1. SNTP Callback ---
static void time_sync_notification_cb(struct timeval *tv)
{
    SYS_LOG("NTP Time successfully synchronized!");
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Set timezone to Central Europe
    tzset();
    localtime_r(&now, &timeinfo);
    
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    SYS_LOG("Current local time: %s", strftime_buf);
}

// --- 2. Network Watchdog Task ---
static void network_watchdog_task(void *pvParameters)
{
    int fail_count = 0;
    SYS_LOG("Network Watchdog started.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); 

        struct hostent *hp = gethostbyname("pool.ntp.org");
        
        if (hp == NULL) {
            fail_count++;
            SYS_LOG_WARN("Watchdog: Internet check failed (%d/3)", fail_count);
            
            if (fail_count >= 3) {
                SYS_LOG_ERR("Watchdog: Network appears dead. Forcing WiFi reconnect...");
                if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
                esp_wifi_disconnect(); 
                fail_count = 0;
            }
        } else {
            fail_count = 0; 
        }
    }
}

// --- 3. Main Event Handler ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        SYS_LOG("WiFi Started. Attempting to connect...");
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        SYS_LOG_WARN("Disconnected from WiFi. Reconnecting...");

        if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_ERROR);
        esp_wifi_connect(); 
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        SYS_LOG("Successfully connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        if (builtin_status_led) sys_led_set_state(builtin_status_led, SYS_LED_STATE_OK_DAY);
        // --- START BACKGROUND SERVICES ONCE WE HAVE AN IP ---

        // A. Start SNTP Time Sync (Non-blocking)
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        esp_sntp_init();

        // B. Start Watchdog Task
        static TaskHandle_t watchdog_handle = NULL;
        if (watchdog_handle == NULL) {
            xTaskCreate(network_watchdog_task, "net_watchdog", 4096, NULL, 5, &watchdog_handle);
        }
    }
}

// --- 4. Initialization ---
void net_core_init(sys_debug_led_t *led_obj) 
{
    builtin_status_led = led_obj; // need to get this to reference builtin led
    SYS_ERR_CHECK(esp_netif_init(), "Failed to initialize TCP/IP stack");
    SYS_ERR_CHECK(esp_event_loop_create_default(), "Failed to create default event loop");
    
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    SYS_ERR_CHECK(esp_netif_set_hostname(sta_netif, CONFIG_WIFI_HOSTNAME), "Failed to set hostname");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    SYS_ERR_CHECK(esp_wifi_init(&cfg), "Failed to init WiFi");

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    SYS_ERR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA), "Failed to set WiFi mode");
    SYS_ERR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), "Failed to set WiFi config");
    SYS_ERR_CHECK(esp_wifi_start(), "Failed to start WiFi");
}