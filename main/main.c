#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "f1_telemetry.h"
#include "udp_receiver.h"

static const char *TAG = "sim_dash";

// ----------------------------------------------------------------
// RPM zone thresholds — percentage of max RPM
// Adjust these to taste once you have real feel for the timing
// ----------------------------------------------------------------
#define REV_OFF_PCT         60   // below this: LED off
#define REV_GREEN_PCT       60   // 60–85%: green
#define REV_RED_PCT         85   // 85–99%: red
#define REV_SHIFT_PCT       99   // 99%+:   purple flash

// ----------------------------------------------------------------
// Rev indicator
// ----------------------------------------------------------------
static void update_rev_indicator(uint16_t rpm, uint16_t max_rpm)
{
    if (max_rpm == 0) {
        led_strip_set_pixel(0, 0, 0, 0);
        return;
    }

    uint8_t rpm_pct = (uint8_t)(((uint32_t)rpm * 100UL) / max_rpm);

    if (rpm_pct >= REV_SHIFT_PCT) {
        // Shift point — flash purple
        static bool flash_state = false;
        flash_state = !flash_state;
        if (flash_state) {
            led_strip_set_pixel(0, 180, 0, 255); // purple
        } else {
            led_strip_set_pixel(0, 0, 0, 0);     // off
        }

    } else if (rpm_pct >= REV_RED_PCT) {
        led_strip_set_pixel(0, 255, 0, 0);       // red

    } else if (rpm_pct >= REV_GREEN_PCT) {
        led_strip_set_pixel(0, 0, 255, 0);       // green

    } else {
        led_strip_set_pixel(0, 0, 0, 0);         // off
    }
}

// ----------------------------------------------------------------
// DRS indicator — placeholder for when the strip arrives.
// With a multi-LED strip, the last LED will show DRS state:
//   dim blue  = DRS allowed but not active
//   bright blue = DRS active (open)
//   off       = DRS not available
// ----------------------------------------------------------------
static void update_drs_indicator(bool allowed, bool active)
{
    // TODO: implement when LED strip arrives and LED_STRIP_LED_COUNT > 1
    (void)allowed;
    (void)active;
}

// ----------------------------------------------------------------
// Tyre wear indicators — placeholder for when the strip arrives.
// With a multi-LED strip, four dedicated LEDs will show wear per
// corner: green (fresh) → yellow → red (worn).
//   tyre_wear_fl, fr, rl, rr are 0–100 (%)
// Note: tyre wear comes from CarDamageData (packet ID 9).
// The f1_telemetry component will need a parser for that packet
// added in a future branch before these values will be non-zero.
// ----------------------------------------------------------------
static void update_tyre_indicators(uint8_t fl, uint8_t fr,
                                    uint8_t rl, uint8_t rr)
{
    // TODO: implement when LED strip arrives and LED_STRIP_LED_COUNT > 1
    (void)fl; (void)fr; (void)rl; (void)rr;
}

// ----------------------------------------------------------------
// Brake bias indicator — placeholder for when the strip arrives.
// With a multi-LED strip, a small bargraph of LEDs will show
// front brake bias percentage (typically 55–65% in F1).
// ----------------------------------------------------------------
static void update_brake_bias_indicator(float bias)
{
    // TODO: implement when LED strip arrives and LED_STRIP_LED_COUNT > 1
    (void)bias;
}

// ----------------------------------------------------------------
// Dashboard update task — runs at ~60fps
// Reads latest telemetry and updates all LED indicators.
// ----------------------------------------------------------------
static void dashboard_task(void *pvParameters)
{
    TelemetryData telem = {0};

    while (1) {
        if (f1_telemetry_get(&telem)) {
            // Rev indicator — active now with single LED
            update_rev_indicator(telem.engine_rpm, telem.max_rpm);

            // All other indicators stubbed until strip arrives
            update_drs_indicator(telem.drs_allowed, telem.drs_active);
            update_tyre_indicators(telem.tyre_wear_fl, telem.tyre_wear_fr,
                                   telem.tyre_wear_rl, telem.tyre_wear_rr);
            update_brake_bias_indicator(telem.brake_bias);

            ESP_LOGI(TAG, "RPM=%d maxRPM=%d pct=%d",
                    telem.engine_rpm, telem.max_rpm,
                    telem.max_rpm > 0 ? (telem.engine_rpm * 100 / telem.max_rpm) : 0);

        } else {
            // No valid telemetry yet — keep LED off
            led_strip_set_pixel(0, 0, 0, 0);
        }

        led_strip_refresh();
        vTaskDelay(pdMS_TO_TICKS(16)); // ~60fps
    }
}

// ----------------------------------------------------------------
// Entry point
// ----------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "Sim Dashboard starting...");

    // Initialise LED strip — must come before any led_strip_set_pixel calls
    led_strip_init();
    led_strip_clear();

    // Initialise telemetry parser in auto-detect mode
    f1_telemetry_init(F1_GAME_AUTO);

    // Connect WiFi and start the UDP receive task.
    // This blocks until WiFi is connected, then returns.
    udp_receiver_start();

    // Start the dashboard render task.
    // Priority 4 — one below the UDP receive task (5) so incoming
    // packets are never delayed by LED rendering.
    xTaskCreate(dashboard_task, "dashboard", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Dashboard running");

    // app_main() returns here — FreeRTOS keeps the tasks alive.
    // This is valid and expected in ESP-IDF applications.
}