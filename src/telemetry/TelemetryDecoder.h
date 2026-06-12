/**
 * @file TelemetryDecoder.h
 * @brief Turns selected MAVLink frames into TelemetryStore updates.
 *
 * Single responsibility: payload interpretation. It is the only place that knows
 * message field layouts, so supporting a new message means editing only here.
 */
#ifndef TELEMETRY_DECODER_H
#define TELEMETRY_DECODER_H

#include "telemetry/IFrameConsumer.h"
#include "telemetry/TelemetryStore.h"

namespace telemetry {

class TelemetryDecoder : public IFrameConsumer {
public:
    explicit TelemetryDecoder(TelemetryStore& store) : store_(store) {}

    void onFrame(const MavlinkFrame& frame) override;

private:
    TelemetryStore& store_;
};

}  // namespace telemetry

#endif /* TELEMETRY_DECODER_H */
