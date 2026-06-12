/**
 * @file LinkManager.h
 * @brief Owns the active byte source + parser and fans frames out to consumers.
 *
 * This is the heart of the LinkTask: poll() pulls bytes from the selected
 * ITelemetrySource, runs them through MavlinkParser, dispatches each complete
 * frame to the registered IFrameConsumers (decoder, forwarder, ...) and keeps
 * the receive-side LinkStats in the TelemetryStore current.
 *
 * Switching UART <-> USB at runtime is a single setSource() call.
 */
#ifndef LINK_MANAGER_H
#define LINK_MANAGER_H

#include <atomic>

#include "telemetry/IFrameConsumer.h"
#include "telemetry/ITelemetrySource.h"
#include "telemetry/MavlinkParser.h"
#include "telemetry/TelemetryStore.h"

namespace telemetry {

class LinkManager {
public:
    explicit LinkManager(TelemetryStore& store) : store_(store) {}

    /** Register a frame consumer (decoder, forwarder, ...). Order = call order. */
    bool addConsumer(IFrameConsumer* consumer);

    /** Select the active physical link; begins it and records its name.
     *  Call only from the LinkTask (e.g. at init). Cross-thread callers should
     *  use requestSource() instead. */
    void setSource(ITelemetrySource* source);
    ITelemetrySource* source() const { return source_; }

    /** Thread-safe request to switch source; applied at the top of the next
     *  poll() on the LinkTask. Safe to call from the UI task. */
    void requestSource(ITelemetrySource* source) { pendingSource_.store(source); }

    /** Drain available bytes, parse, dispatch, refresh link stats. Non-blocking. */
    void poll();

private:
    void dispatch(const MavlinkFrame& frame);
    void pushStats();

    static constexpr int kMaxConsumers   = 4;
    static constexpr int kLinkTimeoutMs  = 3000;

    TelemetryStore&   store_;
    ITelemetrySource* source_ = nullptr;
    MavlinkParser     parser_;

    IFrameConsumer*   consumers_[kMaxConsumers] = {nullptr};
    int               consumerCount_ = 0;

    std::atomic<ITelemetrySource*> pendingSource_{nullptr};
    LinkStats         stats_;
    uint8_t           readBuf_[256];
};

}  // namespace telemetry

#endif /* LINK_MANAGER_H */
