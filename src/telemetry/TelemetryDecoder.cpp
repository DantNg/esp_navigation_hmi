#include "telemetry/TelemetryDecoder.h"

#include <Arduino.h>
#include <cstring>

#include "telemetry/MavlinkIds.h"

namespace telemetry {

using namespace mav;

namespace {
constexpr uint8_t kSafetyArmed = 0x80;  /* MAV_MODE_FLAG_SAFETY_ARMED */
}  // namespace

void TelemetryDecoder::onFrame(const MavlinkFrame& frame) {
    /* MAVLink v2 truncates trailing zero bytes — copy into a zero-filled buffer
     * so every documented field offset is safe to read. */
    uint8_t p[kMaxDecodePayload] = {0};
    uint8_t n = frame.payloadLen;
    if (n > kMaxDecodePayload) n = kMaxDecodePayload;
    memcpy(p, frame.payload(), n);

    const uint32_t now = millis();

    switch (frame.msgid) {
        case HEARTBEAT: {
            const uint8_t  base   = p[heartbeat::base_mode];
            const uint32_t custom = rdU32(p + heartbeat::custom_mode);
            store_.with([&](TelemetrySnapshot& s) {
                s.mode.baseMode     = base;
                s.mode.customMode   = custom;
                s.mode.mavType      = p[heartbeat::type];
                s.mode.autopilot    = p[heartbeat::autopilot];
                s.mode.systemStatus = p[heartbeat::system_status];
                s.mode.armed        = (base & kSafetyArmed) != 0;
                s.mode.updatedMs    = now;
                s.heartbeatSeen     = true;
                s.lastHeartbeatMs   = now;
            });
            break;
        }
        case ATTITUDE: {
            const float roll  = rdF32(p + attitude::roll);
            const float pitch = rdF32(p + attitude::pitch);
            const float yaw   = rdF32(p + attitude::yaw);
            store_.with([&](TelemetrySnapshot& s) {
                s.attitude.roll  = roll;
                s.attitude.pitch = pitch;
                s.attitude.yaw   = yaw;
                s.attitude.updatedMs = now;
            });
            break;
        }
        case SYS_STATUS: {
            const float   volt = rdU16(p + sys_status::voltage_battery) / 1000.0f;
            const int16_t curR = rdI16(p + sys_status::current_battery);
            const int8_t  rem  = (int8_t)p[sys_status::battery_remaining];
            store_.with([&](TelemetrySnapshot& s) {
                s.battery.voltage   = volt;
                s.battery.current   = (curR < 0) ? 0.0f : (curR / 100.0f);
                s.battery.remaining = rem;
                s.battery.updatedMs = now;
            });
            break;
        }
        case BATTERY_STATUS: {
            const int8_t rem = (int8_t)p[battery_status::battery_remaining];
            store_.with([&](TelemetrySnapshot& s) {
                if (rem >= 0) s.battery.remaining = rem;
                s.battery.updatedMs = now;
            });
            break;
        }
        case GPS_RAW_INT: {
            const uint8_t  fix = p[gps_raw::fix_type];
            const uint8_t  sat = p[gps_raw::satellites_visible];
            const uint16_t eph = rdU16(p + gps_raw::eph);
            store_.with([&](TelemetrySnapshot& s) {
                s.gps.fixType    = fix;
                s.gps.satellites = sat;
                s.gps.hdop       = (eph == 0xFFFF) ? 0.0f : (eph / 100.0f);
                s.gps.updatedMs  = now;
            });
            break;
        }
        case GLOBAL_POSITION_INT: {
            const double   lat = rdI32(p + global_pos::lat) / 1e7;
            const double   lon = rdI32(p + global_pos::lon) / 1e7;
            const float    amsl = rdI32(p + global_pos::alt) / 1000.0f;
            const float    arel = rdI32(p + global_pos::relative_alt) / 1000.0f;
            const uint16_t hdg  = rdU16(p + global_pos::hdg);
            store_.with([&](TelemetrySnapshot& s) {
                s.position.lat        = lat;
                s.position.lon        = lon;
                s.position.altMsl     = amsl;
                s.position.altRel     = arel;
                s.position.headingDeg = (hdg == 0xFFFF) ? s.position.headingDeg
                                                        : (hdg / 100.0f);
                s.position.valid      = (lat != 0.0 || lon != 0.0);
                s.position.updatedMs  = now;
            });
            break;
        }
        case VFR_HUD: {
            const float as = rdF32(p + vfr_hud::airspeed);
            const float gs = rdF32(p + vfr_hud::groundspeed);
            const float cl = rdF32(p + vfr_hud::climb);
            const uint16_t thr = rdU16(p + vfr_hud::throttle);
            store_.with([&](TelemetrySnapshot& s) {
                s.vfr.airspeed    = as;
                s.vfr.groundspeed = gs;
                s.vfr.climb       = cl;
                s.vfr.throttle    = thr;
                s.vfr.updatedMs   = now;
            });
            break;
        }
        case STATUSTEXT: {
            StatusText st;
            st.severity = (Severity)p[statustext::severity];
            memcpy(st.text, p + statustext::text, statustext::text_len);
            st.text[statustext::text_len] = '\0';
            st.updatedMs = now;
            st.valid     = true;
            store_.with([&](TelemetrySnapshot& s) { s.status = st; });
            break;
        }
        default:
            break;
    }
}

}  // namespace telemetry
