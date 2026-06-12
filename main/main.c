#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "f1_telemetry.h"
#include "udp_receiver.h"
#include "esp_log.h"

static const char *TAG = "sim-dash";

void app_main(void)
{
    // *** LED STRIP TEST ***
    // led_strip_init();
    // led_strip_clear();

    // while (1) {
    //     // Red for 1 second
    //     led_strip_set_pixel(0, 255, 0, 0);
    //     led_strip_refresh();
    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     // Green for 1 second
    //     led_strip_set_pixel(0, 0, 255, 0);
    //     led_strip_refresh();
    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     // Blue for 1 second
    //     led_strip_set_pixel(0, 0, 0, 255);
    //     led_strip_refresh();
    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     // Off for half a second
    //     led_strip_clear();
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }

    // *** TELEMETRY DATA TEST ***

    // f1_telemetry_init(F1_GAME_AUTO);

    // // Build a minimal fake F1 2021 CarTelemetryData packet.
    // // We only need to fill in the fields we actually parse.
    // // Total size must match sizeof(F1_21_PacketCarTelemetryData).
    // // We use a zeroed buffer and set specific fields by offset.
    // //
    // // Header layout (packed, 24 bytes):
    // //   [0-1]  packetFormat = 2021 (0x07E5)
    // //   [2]    gameMajorVersion
    // //   [3]    gameMinorVersion
    // //   [4]    packetVersion
    // //   [5]    packetId = 6 (CAR_TELEMETRY)
    // //   [6-13] sessionUID (8 bytes)
    // //   [14-17] sessionTime (float)
    // //   [18-21] frameIdentifier
    // //   [22]   playerCarIndex = 0
    // //   [23]   secondaryPlayerCarIndex

    // uint8_t fake_packet[1347] = {0}; // sizeof(F1_21_PacketCarTelemetryData)

    // // packetFormat = 2021 (little-endian)
    // fake_packet[0] = 0xE5;
    // fake_packet[1] = 0x07;

    // // packetId = 6 (CAR_TELEMETRY)
    // fake_packet[5] = 6;

    // // playerCarIndex = 0
    // fake_packet[22] = 0;

    // // Car 0 telemetry data starts at byte 24 (after the 24-byte header)
    // // CarTelemetryData layout (packed):
    // //   [0-1]  speed (uint16)
    // //   [2]    throttle_raw (uint8)
    // //   [3]    steer (int8)
    // //   [4]    brake_raw (uint8)
    // //   [5]    clutch (uint8)
    // //   [6]    gear (int8)
    // //   [7-8]  engineRPM (uint16)
    // //   [9]    drs (uint8)
    // //   ... rest zeroed

    // uint32_t car0_offset = 24; // header is 24 bytes

    // // speed = 200 km/h (little-endian)
    // fake_packet[car0_offset + 0] = 200;
    // fake_packet[car0_offset + 1] = 0;

    // // throttle = 204 (raw 0-255, = ~80%)
    // fake_packet[car0_offset + 2] = 204;

    // // brake = 0
    // fake_packet[car0_offset + 4] = 0;

    // // gear = 6
    // fake_packet[car0_offset + 6] = 6;

    // // engineRPM = 11500 (0x2CEC little-endian)
    // fake_packet[car0_offset + 7] = 0xEC;
    // fake_packet[car0_offset + 8] = 0x2C;

    // // drs = 1 (active)
    // fake_packet[car0_offset + 9] = 1;

    // // Parse the fake packet
    // f1_telemetry_parse(fake_packet, sizeof(fake_packet));

    // // Read back and verify
    // TelemetryData telem = {0};
    // if (f1_telemetry_get(&telem)) {
    //     ESP_LOGI(TAG, "Parse OK:");
    //     ESP_LOGI(TAG, "  RPM      = %d (expected 11500)", telem.engine_rpm);
    //     ESP_LOGI(TAG, "  Gear     = %d (expected 6)",     telem.gear);
    //     ESP_LOGI(TAG, "  Throttle = %d%% (expected ~80)", telem.throttle);
    //     ESP_LOGI(TAG, "  Brake    = %d%% (expected 0)",   telem.brake);
    //     ESP_LOGI(TAG, "  DRS      = %d (expected 1)",     telem.drs_active);
    // } else {
    //     ESP_LOGE(TAG, "f1_telemetry_get returned false — no valid data");
    // }

    // // Keep the task alive so the monitor stays connected
    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(5000));
    // }

    // *** UDP Received Test ***
    // Initialise telemetry parser in auto-detect mode
    f1_telemetry_init(F1_GAME_AUTO);

    // Connect to WiFi and start the UDP receive task
    udp_receiver_start();

    // Poll telemetry data every 500ms and print to serial monitor
    TelemetryData telem = {0};
    while (1) {
        if (f1_telemetry_get(&telem)) {
            ESP_LOGI(TAG, "RPM=%d/%d gear=%d throttle=%d%% brake=%d%% drs=%d/%d bias=%.1f",
                     telem.engine_rpm, telem.max_rpm,
                     telem.gear,
                     telem.throttle, telem.brake,
                     telem.drs_active, telem.drs_allowed,
                     telem.brake_bias);
        } else {
            ESP_LOGI(TAG, "Waiting for telemetry...");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}