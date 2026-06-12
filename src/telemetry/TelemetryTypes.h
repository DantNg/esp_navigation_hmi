/**
 * @file TelemetryTypes.h
 * @brief Plain-old-data describing the drone state we display.
 *
 * These structs are the shared vocabulary between the decoder (writer) and the
 * UI / map (readers). They carry no behaviour — pure data, copied by value.
 */
#ifndef TELEMETRY_TYPES_H
#define TELEMETRY_TYPES_H

#include <cstdint>

namespace telemetry {

/** Attitude from ATTITUDE (radians). */
struct Attitude {
    float    roll  = 0;   /* rad */
    float    pitch = 0;   /* rad */
    float    yaw   = 0;   /* rad */
    uint32_t updatedMs = 0;
};

/** Battery from SYS_STATUS / BATTERY_STATUS. */
struct Battery {
    float   voltage   = 0;    /* V    */
    float   current   = 0;    /* A    */
    int8_t  remaining = -1;   /* %, -1 = unknown */
    uint32_t updatedMs = 0;
};

/** Position from GLOBAL_POSITION_INT (+ GPS_RAW_INT for fix quality). */
struct GeoPosition {
    double  lat       = 0;    /* deg */
    double  lon       = 0;    /* deg */
    float   altMsl    = 0;    /* m   */
    float   altRel    = 0;    /* m   */
    float   headingDeg = 0;   /* deg */
    bool    valid     = false;
    uint32_t updatedMs = 0;
};

/** Air data from VFR_HUD (speeds + climb). */
struct Vfr {
    float    airspeed    = 0;  /* m/s */
    float    groundspeed = 0;  /* m/s */
    float    climb       = 0;  /* m/s */
    uint16_t throttle    = 0;  /* %   */
    uint32_t updatedMs   = 0;
};

/** GPS link quality from GPS_RAW_INT. */
struct GpsInfo {
    uint8_t fixType    = 0;   /* 0-1 none, 2 = 2D, 3 = 3D, ... */
    uint8_t satellites = 0;
    float   hdop       = 0;
    uint32_t updatedMs = 0;
};

/** Mode / arming from HEARTBEAT. */
struct FlightModeInfo {
    uint8_t  baseMode     = 0;
    uint32_t customMode   = 0;
    uint8_t  mavType      = 0;   /* MAV_TYPE_*      */
    uint8_t  autopilot    = 0;   /* MAV_AUTOPILOT_* */
    uint8_t  systemStatus = 0;   /* MAV_STATE_*     */
    bool     armed        = false;
    uint32_t updatedMs    = 0;
};

/** Receive-side link health (maintained by LinkManager). */
struct LinkStats {
    uint32_t framesReceived = 0;
    uint32_t bytesReceived  = 0;
    uint32_t parseErrors    = 0;
    uint32_t lastFrameMs    = 0;
    bool     linkUp         = false;
    char     sourceName[12] = "—";
};

/** Severity levels of STATUSTEXT (MAV_SEVERITY). */
enum class Severity : uint8_t {
    Emergency = 0, Alert = 1, Critical = 2, Error = 3,
    Warning = 4, Notice = 5, Info = 6, Debug = 7
};

/** Last STATUSTEXT warning/notice. */
struct StatusText {
    Severity severity = Severity::Info;
    char     text[51] = {0};
    uint32_t updatedMs = 0;
    bool     valid    = false;
};

/** Full snapshot copied atomically out of TelemetryStore. */
struct TelemetrySnapshot {
    Attitude       attitude;
    Battery        battery;
    GeoPosition    position;
    GpsInfo        gps;
    Vfr            vfr;
    FlightModeInfo mode;
    LinkStats      link;
    StatusText     status;
    bool           heartbeatSeen   = false;
    uint32_t       lastHeartbeatMs = 0;
};

}  // namespace telemetry

#endif /* TELEMETRY_TYPES_H */
