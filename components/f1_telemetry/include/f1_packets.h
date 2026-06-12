#pragma once

#include <stdint.h>

// ----------------------------------------------------------------
// F1 2021 UDP packet structures
// Source: EA/Codemasters F1 2021 UDP Spec
// All structs use __attribute__((packed)) to prevent compiler
// padding — the binary layout must exactly match what the game sends.
// ----------------------------------------------------------------

// Every F1 21 packet starts with this 24-byte header
typedef struct __attribute__((packed)) {
    uint16_t m_packetFormat;        // 2021
    uint8_t  m_gameMajorVersion;
    uint8_t  m_gameMinorVersion;
    uint8_t  m_packetVersion;
    uint8_t  m_packetId;            // identifies packet type (see IDs below)
    uint64_t m_sessionUID;
    float    m_sessionTime;
    uint32_t m_frameIdentifier;
    uint8_t  m_playerCarIndex;      // index of player's car in data arrays
    uint8_t  m_secondaryPlayerCarIndex;
} F1_21_PacketHeader;

// Packet IDs
#define F1_21_PACKET_MOTION         0
#define F1_21_PACKET_SESSION        1
#define F1_21_PACKET_LAP_DATA       2
#define F1_21_PACKET_EVENT          3
#define F1_21_PACKET_PARTICIPANTS   4
#define F1_21_PACKET_CAR_SETUPS     5
#define F1_21_PACKET_CAR_TELEMETRY  6
#define F1_21_PACKET_CAR_STATUS     7
#define F1_21_PACKET_FINAL_CLASS    8
#define F1_21_PACKET_CAR_DAMAGE     9
#define F1_21_PACKET_SESSION_HIST   10

// Per-car telemetry data (one entry in the array of 22)
typedef struct __attribute__((packed)) {
    uint16_t m_speed;               // speed in km/h
    float    m_throttle;            // 0.0 to 1.0
    float    m_steer;               // -1.0 to 1.0
    float    m_brake;               // 0.0 to 1.0
    uint8_t  m_clutch;              // 0 to 100
    int8_t   m_gear;                // -1=reverse, 0=neutral, 1-8=gears
    uint16_t m_engineRPM;
    uint8_t  m_drs;                 // 0=off, 1=on
    uint8_t  m_revLightsPercent;
    uint16_t m_revLightsBitValue;
    uint16_t m_brakesTemperature[4];
    uint8_t  m_tyresSurfaceTemperature[4];
    uint8_t  m_tyresInnerTemperature[4];
    uint16_t m_engineTemperature;
    float    m_tyresPressure[4];
    uint8_t  m_surfaceType[4];
} F1_21_CarTelemetryData;

// Full car telemetry packet (header + array of 22 cars + extras)
typedef struct __attribute__((packed)) {
    F1_21_PacketHeader     m_header;
    F1_21_CarTelemetryData m_carTelemetryData[22];
    uint8_t  m_mfdPanelIndex;
    uint8_t  m_mfdPanelIndexSecondaryPlayer;
    int8_t   m_suggestedGear;
} F1_21_PacketCarTelemetryData;

// Per-car status data (one entry in the array of 22)
typedef struct __attribute__((packed)) {
    uint8_t  m_tractionControl;
    uint8_t  m_antiLockBrakes;
    uint8_t  m_fuelMix;
    uint8_t  m_frontBrakeBias;      // brake bias front % (0–100)
    uint8_t  m_pitLimiterStatus;
    float    m_fuelInTank;
    float    m_fuelCapacity;
    float    m_fuelRemainingLaps;
    uint16_t m_maxRPM;              // rev limiter RPM
    uint16_t m_idleRPM;
    uint8_t  m_maxGears;
    uint8_t  m_drsAllowed;          // 0=not allowed, 1=allowed
    uint16_t m_drsActivationDistance;
    uint8_t  m_actualTyreCompound;
    uint8_t  m_visualTyreCompound;
    uint8_t  m_tyresAgeLaps;
    int8_t   m_vehicleFiaFlags;
    float    m_ersStoreEnergy;
    uint8_t  m_ersDeployMode;
    float    m_ersHarvestedThisLapMGUK;
    float    m_ersHarvestedThisLapMGUH;
    float    m_ersDeployedThisLap;
    uint8_t  m_networkPaused;
} F1_21_CarStatusData;

// Full car status packet
typedef struct __attribute__((packed)) {
    F1_21_PacketHeader   m_header;
    F1_21_CarStatusData  m_carStatusData[22];
} F1_21_PacketCarStatusData;