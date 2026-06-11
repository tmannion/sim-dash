#pragma once

#include <stdint.h>
#include <stdbool.h>

// ----------------------------------------------------------------
// Common telemetry data structure
// This is the ONLY struct the rest of the project interacts with.
// All game-specific parsing fills this struct.
// ----------------------------------------------------------------
typedef struct {
    uint16_t engine_rpm;        // current engine RPM
    uint16_t max_rpm;           // rev limiter RPM
    uint8_t  gear;              // current gear (0=reverse, 1-8=gears)
    uint8_t  throttle;          // throttle pedal 0–100 (%)
    uint8_t  brake;             // brake pedal 0–100 (%)
    bool     drs_active;        // DRS currently open
    bool     drs_allowed;       // DRS available to activate
    uint8_t  tyre_wear_fl;      // front-left tyre wear 0–100 (%)
    uint8_t  tyre_wear_fr;      // front-right tyre wear 0–100 (%)
    uint8_t  tyre_wear_rl;      // rear-left tyre wear 0–100 (%)
    uint8_t  tyre_wear_rr;      // rear-right tyre wear 0–100 (%)
    float    brake_bias;        // brake bias front % (e.g. 56.0)
    bool     valid;             // true if this struct contains fresh data
} TelemetryData;

// Supported game versions
typedef enum {
    F1_GAME_AUTO   = 0,  // detect from packet header (recommended)
    F1_GAME_2021   = 1,
    F1_GAME_2023   = 2,
} F1GameVersion;

// Initialise the telemetry parser.
// game_version: pass F1_GAME_AUTO to detect automatically from packet headers.
void f1_telemetry_init(F1GameVersion game_version);

// Parse raw UDP bytes into the shared TelemetryData struct.
// data: pointer to the raw packet bytes
// length: number of bytes received
void f1_telemetry_parse(const uint8_t *data, uint32_t length);

// Get a copy of the most recently parsed telemetry data.
// Returns false if no valid data has been parsed yet.
bool f1_telemetry_get(TelemetryData *out);