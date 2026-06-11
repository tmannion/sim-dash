#include "f1_telemetry.h"
#include "f1_packets.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "f1_telemetry";

// Shared state — written by the parser, read by f1_telemetry_get()
static TelemetryData  s_telemetry = {0};
static F1GameVersion  s_game_version = F1_GAME_AUTO;

// Detected game version (populated on first packet when AUTO mode)
static F1GameVersion  s_detected_version = F1_GAME_AUTO;

// ----------------------------------------------------------------
// F1 2021 parsers
// ----------------------------------------------------------------

static void parse_f1_2021_telemetry(const uint8_t *data, uint32_t length)
{
    if (length < sizeof(F1_21_PacketCarTelemetryData)) {
        ESP_LOGW(TAG, "F1_21 telemetry packet too short: %lu bytes", (unsigned long)length);
        return;
    }

    const F1_21_PacketCarTelemetryData *pkt = (const F1_21_PacketCarTelemetryData *)data;

    uint8_t idx = pkt->m_header.m_playerCarIndex;
    if (idx >= 22) {
        ESP_LOGW(TAG, "Invalid player car index: %d", idx);
        return;
    }

    const F1_21_CarTelemetryData *car = &pkt->m_carTelemetryData[idx];

    s_telemetry.engine_rpm = car->m_engineRPM;
    s_telemetry.gear       = (uint8_t)(car->m_gear < 0 ? 0 : car->m_gear);
    // Convert raw 0-255 values to 0-100 percentage
    s_telemetry.throttle   = (uint8_t)((car->m_throttle_raw / 255.0f) * 100.0f);
    s_telemetry.brake      = (uint8_t)((car->m_brake_raw    / 255.0f) * 100.0f);
    s_telemetry.drs_active = (car->m_drs == 1);
    s_telemetry.valid      = true;

    ESP_LOGD(TAG, "Telemetry: RPM=%d gear=%d throttle=%d%% brake=%d%% drs=%d",
             s_telemetry.engine_rpm, s_telemetry.gear,
             s_telemetry.throttle,   s_telemetry.brake,
             s_telemetry.drs_active);
}

static void parse_f1_2021_status(const uint8_t *data, uint32_t length)
{
    if (length < sizeof(F1_21_PacketCarStatusData)) {
        ESP_LOGW(TAG, "F1_21 status packet too short: %lu bytes", (unsigned long)length);
        return;
    }

    const F1_21_PacketCarStatusData *pkt = (const F1_21_PacketCarStatusData *)data;

    uint8_t idx = pkt->m_header.m_playerCarIndex;
    if (idx >= 22) return;

    const F1_21_CarStatusData *car = &pkt->m_carStatusData[idx];

    s_telemetry.max_rpm     = car->m_maxRPM;
    s_telemetry.drs_allowed = (car->m_drsAllowed == 1);
    s_telemetry.brake_bias  = (float)car->m_frontBrakeBias;

    // Tyre wear is not in CarStatusData in F1 21 — it's in CarDamageData (packet ID 9).
    // We will add that parser in Phase 2. For now these stay at 0.

    ESP_LOGD(TAG, "Status: maxRPM=%d drsAllowed=%d brakeBias=%.1f",
             s_telemetry.max_rpm, s_telemetry.drs_allowed, s_telemetry.brake_bias);
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

void f1_telemetry_init(F1GameVersion game_version)
{
    s_game_version = game_version;
    s_detected_version = F1_GAME_AUTO;
    memset(&s_telemetry, 0, sizeof(s_telemetry));
    ESP_LOGI(TAG, "Telemetry parser initialised (mode: %d)", game_version);
}

void f1_telemetry_parse(const uint8_t *data, uint32_t length)
{
    // Every packet starts with the header — need at least that
    if (length < sizeof(F1_21_PacketHeader)) {
        ESP_LOGW(TAG, "Packet too short for header: %lu bytes", (unsigned long)length);
        return;
    }

    // Read the packet format version from the header to detect game
    const F1_21_PacketHeader *header = (const F1_21_PacketHeader *)data;
    uint16_t format = header->m_packetFormat;

    // Determine which game version to parse as
    F1GameVersion version = s_game_version;
    if (version == F1_GAME_AUTO) {
        if (format == 2021) {
            version = F1_GAME_2021;
        } else if (format == 2023) {
            version = F1_GAME_2023;
        } else {
            ESP_LOGW(TAG, "Unknown packet format: %d", format);
            return;
        }
        if (s_detected_version != version) {
            s_detected_version = version;
            ESP_LOGI(TAG, "Auto-detected game: F1 %d", format);
        }
    }

    // Dispatch to the correct game parser based on packet ID
    if (version == F1_GAME_2021) {
        switch (header->m_packetId) {
            case F1_21_PACKET_CAR_TELEMETRY:
                parse_f1_2021_telemetry(data, length);
                break;
            case F1_21_PACKET_CAR_STATUS:
                parse_f1_2021_status(data, length);
                break;
            default:
                // Silently ignore packets we don't need
                break;
        }
    } else if (version == F1_GAME_2023) {
        // F1 2023 parser will be added here in a future branch
        ESP_LOGW(TAG, "F1 2023 parser not yet implemented");
    }
}

bool f1_telemetry_get(TelemetryData *out)
{
    if (!s_telemetry.valid) return false;
    memcpy(out, &s_telemetry, sizeof(TelemetryData));
    return true;
}