/**
 * @file IFrameConsumer.h
 * @brief Observer interface for completed MAVLink frames.
 *
 * LinkManager publishes every parsed frame to a list of consumers. This keeps
 * the parser closed for modification: new behaviour (decode, forward, log) is
 * added by writing a new IFrameConsumer, not by editing LinkManager.
 */
#ifndef I_FRAME_CONSUMER_H
#define I_FRAME_CONSUMER_H

#include "telemetry/MavlinkParser.h"

namespace telemetry {

class IFrameConsumer {
public:
    virtual ~IFrameConsumer() = default;
    virtual void onFrame(const MavlinkFrame& frame) = 0;
};

}  // namespace telemetry

#endif /* I_FRAME_CONSUMER_H */
