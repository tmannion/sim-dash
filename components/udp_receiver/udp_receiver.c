#include "udp_receiver.h"
#include "f1_telemetry.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "udp_receiver";

// Maximum UDP packet size for F1 21 — largest packet is motion data at ~1464 bytes
#define UDP_RX_BUF_SIZE     1500

// Event group bit — set when WiFi connection + IP assignment is complete
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_count = 0;
#define WIFI_MAX_RETRIES    5

// ----------------------------------------------------------------
// WiFi event handler
// Called by the event loop when WiFi state changes.
// ----------------------------------------------------------------
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // WiFi driver started — attempt to connect
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRIES) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "WiFi disconnected, retrying (%d/%d)...",
                     s_retry_count, WIFI_MAX_RETRIES);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi connection failed after %d attempts", WIFI_MAX_RETRIES);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Got an IP address — connection is fully established
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ----------------------------------------------------------------
// WiFi initialisation — blocks until connected or failed
// ----------------------------------------------------------------
static void wifi_init(void)
{
    // NVS is required by the WiFi driver to store calibration data
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or has a version mismatch — erase and re-init
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    // Initialise the TCP/IP network stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop (required for WiFi events)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create the default WiFi station interface
    esp_netif_create_default_wifi_sta();

    // Initialise the WiFi driver with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Create event group to synchronise with connection completion
    s_wifi_event_group = xEventGroupCreate();

    // Register event handlers for WiFi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Configure WiFi with credentials from menuconfig
    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = CONFIG_UDP_WIFI_SSID,
            .password = CONFIG_UDP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s ...", CONFIG_UDP_WIFI_SSID);

    // Block here until connected or failed
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully");
    } else {
        ESP_LOGE(TAG, "WiFi failed — check SSID/password in menuconfig");
        // In a real product you'd handle this gracefully; for development, abort
        abort();
    }
}

// ----------------------------------------------------------------
// UDP receive task — runs forever in the background
// ----------------------------------------------------------------
static void udp_receive_task(void *pvParameters)
{
    uint8_t rx_buf[UDP_RX_BUF_SIZE];

    // Create a UDP socket
    // AF_INET = IPv4, SOCK_DGRAM = UDP, IPPROTO_UDP = explicit UDP
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Bind to all interfaces on the telemetry port
    struct sockaddr_in bind_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(CONFIG_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY), // listen on all network interfaces
    };

    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Listening for F1 telemetry on UDP port %d", CONFIG_UDP_PORT);

    // Receive loop — blocks on recvfrom() until a packet arrives
    while (1) {
        struct sockaddr_in src_addr;
        socklen_t src_addr_len = sizeof(src_addr);

        int len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0,
                           (struct sockaddr *)&src_addr, &src_addr_len);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom error: errno %d", errno);
            break; // exit loop on error, task will be deleted
        }

        // Pass raw bytes directly to the telemetry parser
        f1_telemetry_parse(rx_buf, (uint32_t)len);
    }

    close(sock);
    vTaskDelete(NULL); // delete this task cleanly
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

void udp_receiver_start(void)
{
    wifi_init();

    // Create the receive task
    // Stack size of 4096 bytes is sufficient for this task
    // Priority 5 is above the idle task (0) but below time-critical tasks
    xTaskCreate(udp_receive_task, "udp_rx", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "UDP receive task started");
}