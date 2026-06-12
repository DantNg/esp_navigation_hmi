#include "telemetry/LinkManager.h"

#include <Arduino.h>
#include <cstring>

namespace telemetry {

bool LinkManager::addConsumer(IFrameConsumer* consumer) {
    if (!consumer || consumerCount_ >= kMaxConsumers) return false;
    consumers_[consumerCount_++] = consumer;
    return true;
}

void LinkManager::setSource(ITelemetrySource* source) {
    source_ = source;
    parser_.reset();
    if (source_) {
        source_->begin();
        strncpy(stats_.sourceName, source_->name(), sizeof(stats_.sourceName) - 1);
        stats_.sourceName[sizeof(stats_.sourceName) - 1] = '\0';
    }
    stats_.linkUp = false;
    pushStats();
}

void LinkManager::poll() {
    /* Apply a pending cross-thread source switch (from the UI task). */
    if (ITelemetrySource* pending = pendingSource_.exchange(nullptr)) {
        if (pending != source_) setSource(pending);
    }

    if (!source_) return;

    const int n = source_->read(readBuf_, sizeof(readBuf_));
    const uint32_t now = millis();

    if (n > 0) {
        stats_.bytesReceived += (uint32_t)n;
        for (int i = 0; i < n; i++) {
            MavlinkFrame frame;
            if (parser_.parse(readBuf_[i], frame, &stats_.parseErrors)) {
                stats_.framesReceived++;
                stats_.lastFrameMs = now;
                stats_.linkUp = true;
                dispatch(frame);
            }
        }
        pushStats();
    } else if (stats_.linkUp && (now - stats_.lastFrameMs > kLinkTimeoutMs)) {
        stats_.linkUp = false;
        pushStats();
    }
}

void LinkManager::dispatch(const MavlinkFrame& frame) {
    for (int i = 0; i < consumerCount_; i++) {
        consumers_[i]->onFrame(frame);
    }
}

void LinkManager::pushStats() {
    store_.with([&](TelemetrySnapshot& s) { s.link = stats_; });
}

}  // namespace telemetry
