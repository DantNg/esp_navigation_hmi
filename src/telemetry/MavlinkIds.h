/**
 * @file MavlinkIds.h
 * @brief Message IDs, wire field offsets and little-endian readers.
 *
 * We hard-code only the handful of messages the ground station displays instead
 * of pulling the multi-thousand-file generated MAVLink library. Field offsets
 * follow the MAVLink wire layout (fields ordered by descending type size).
 *
 * NOTE: MAVLink v2 may truncate trailing zero bytes of a payload, so a received
 * payload can be shorter than the struct. Callers must zero-pad before reading
 * (see TelemetryDecoder), then these readers are always safe.
 */
#ifndef MAVLINK_IDS_H
#define MAVLINK_IDS_H

#include <cstdint>
#include <cstring>

namespace telemetry {
namespace mav {

/* ---- Message IDs ---- */
enum MsgId : uint32_t {
    HEARTBEAT           = 0,
    SYS_STATUS          = 1,
    GPS_RAW_INT         = 24,
    ATTITUDE            = 30,
    GLOBAL_POSITION_INT = 33,
    VFR_HUD             = 74,
    BATTERY_STATUS      = 147,
    STATUSTEXT          = 253,
};

/* Largest payload we copy/zero-pad for decoding. */
constexpr int kMaxDecodePayload = 60;

/* ---- Field offsets (bytes into payload) ---- */
namespace heartbeat {            /* len 9 */
    constexpr int custom_mode   = 0;   /* u32 */
    constexpr int type          = 4;   /* u8  */
    constexpr int autopilot     = 5;   /* u8  */
    constexpr int base_mode     = 6;   /* u8  */
    constexpr int system_status = 7;   /* u8  */
}
namespace sys_status {           /* len 31 */
    constexpr int voltage_battery   = 14;  /* u16  mV  */
    constexpr int current_battery   = 16;  /* i16  cA  */
    constexpr int battery_remaining = 30;  /* i8   %   */
}
namespace gps_raw {              /* len 30 */
    constexpr int fix_type           = 28;  /* u8 */
    constexpr int satellites_visible = 29;  /* u8 */
    constexpr int eph                = 20;  /* u16 (cm) */
}
namespace attitude {             /* len 28 */
    constexpr int roll  = 4;   /* f32 rad */
    constexpr int pitch = 8;   /* f32 rad */
    constexpr int yaw   = 12;  /* f32 rad */
}
namespace global_pos {           /* len 28 */
    constexpr int lat          = 4;   /* i32 1e7 deg  */
    constexpr int lon          = 8;   /* i32 1e7 deg  */
    constexpr int alt          = 12;  /* i32 mm MSL   */
    constexpr int relative_alt = 16;  /* i32 mm       */
    constexpr int hdg          = 26;  /* u16 cdeg     */
}
namespace battery_status {       /* len 36 */
    constexpr int voltage0           = 10;  /* u16 mV (cell 0) */
    constexpr int current_battery    = 30;  /* i16 cA */
    constexpr int battery_remaining  = 35;  /* i8  %  */
}
namespace vfr_hud {              /* len 20 */
    constexpr int airspeed    = 0;   /* f32 m/s */
    constexpr int groundspeed = 4;   /* f32 m/s */
    constexpr int alt         = 8;   /* f32 m   */
    constexpr int climb       = 12;  /* f32 m/s */
    constexpr int heading     = 16;  /* i16 deg */
    constexpr int throttle    = 18;  /* u16 %   */
}
namespace statustext {           /* len 51 (v1) */
    constexpr int severity = 0;   /* u8        */
    constexpr int text     = 1;   /* char[50]  */
    constexpr int text_len = 50;
}

/* ---- Little-endian readers ---- */
inline uint16_t rdU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
inline int16_t rdI16(const uint8_t* p) { return (int16_t)rdU16(p); }

inline uint32_t rdU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline int32_t rdI32(const uint8_t* p) { return (int32_t)rdU32(p); }

inline float rdF32(const uint8_t* p) {
    float f;
    uint32_t u = rdU32(p);
    memcpy(&f, &u, sizeof(f));
    return f;
}

}  // namespace mav
}  // namespace telemetry

#endif /* MAVLINK_IDS_H */
