/**
 * @file FlightMode.h
 * @brief Best-effort human name for a HEARTBEAT custom_mode.
 *
 * MAVLink custom_mode is autopilot-specific. We resolve the common ArduPilot
 * Copter table (the typical hobby case) and fall back to a numeric label for
 * everything else, so the UI always shows *something* sensible.
 */
#ifndef FLIGHT_MODE_H
#define FLIGHT_MODE_H

#include <cstdint>
#include <cstdio>

#include "telemetry/TelemetryTypes.h"

namespace telemetry {

namespace {
constexpr uint8_t kMavAutopilotArduPilot = 3;  /* MAV_AUTOPILOT_ARDUPILOTMEGA */
}

/** Returns a static or caller-buffer string naming the flight mode. */
inline const char* flightModeName(const FlightModeInfo& m, char* scratch, size_t n) {
    if (m.autopilot == kMavAutopilotArduPilot) {
        switch (m.customMode) {
            case 0:  return "STABILIZE";
            case 1:  return "ACRO";
            case 2:  return "ALT_HOLD";
            case 3:  return "AUTO";
            case 4:  return "GUIDED";
            case 5:  return "LOITER";
            case 6:  return "RTL";
            case 7:  return "CIRCLE";
            case 9:  return "LAND";
            case 11: return "DRIFT";
            case 13: return "SPORT";
            case 14: return "FLIP";
            case 15: return "AUTOTUNE";
            case 16: return "POSHOLD";
            case 17: return "BRAKE";
            case 18: return "THROW";
            case 20: return "GUIDED_NOGPS";
            case 21: return "SMART_RTL";
            case 23: return "FOLLOW";
            case 24: return "ZIGZAG";
            case 27: return "AUTO_RTL";
            default: break;
        }
    }
    snprintf(scratch, n, "MODE %lu", (unsigned long)m.customMode);
    return scratch;
}

}  // namespace telemetry

#endif /* FLIGHT_MODE_H */
