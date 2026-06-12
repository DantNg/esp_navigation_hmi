/**
 * @file UdpMavlinkForwarder.h
 * @brief Forwards raw MAVLink frames to a real ground station over UDP.
 *
 * It plays two small roles joined by a queue:
 *   - IFrameConsumer (runs on LinkTask): when forwarding is enabled, copy the
 *     raw frame into a FreeRTOS queue. This keeps the link hot-path cheap.
 *   - IFrameForwarder (runs on ForwardTask): drain the queue and UDP-send each
 *     frame byte-for-byte to gcsIp:gcsPort, so Mission Planner / QGroundControl
 *     receive an unmodified stream.
 *
 * Enabling/disabling is an atomic flag, safe to flip from the UI task.
 */
#ifndef UDP_MAVLINK_FORWARDER_H
#define UDP_MAVLINK_FORWARDER_H

#include <WiFiUdp.h>
#include <atomic>

#include "forward/IFrameForwarder.h"
#include "net/WifiManager.h"
#include "telemetry/IFrameConsumer.h"

namespace forward {

class UdpMavlinkForwarder : public telemetry::IFrameConsumer,
                            public IFrameForwarder {
public:
    explicit UdpMavlinkForwarder(net::WifiManager& wifi) : wifi_(wifi) {}

    /** Set destination (called before start(), or to retarget). */
    void configure(const char* ip, uint16_t port);

    /** Create the queue and spawn the ForwardTask (core 0). */
    void start();

    void setEnabled(bool enabled) { enabled_.store(enabled); }
    bool enabled() const          { return enabled_.load(); }
    uint32_t framesForwarded() const { return forwarded_.load(); }

    /* IFrameConsumer — runs on LinkTask. */
    void onFrame(const telemetry::MavlinkFrame& frame) override;

    /* IFrameForwarder — runs on ForwardTask. */
    bool send(const uint8_t* data, size_t len) override;

private:
    struct Item {
        uint16_t len;
        uint8_t  data[telemetry::kMavMaxFrame];
    };

    static void taskTrampoline(void* arg);
    void run();

    net::WifiManager&     wifi_;
    WiFiUDP               udp_;
    IPAddress             target_;
    uint16_t              port_     = 14550;
    std::atomic<bool>     enabled_{false};
    std::atomic<uint32_t> forwarded_{0};
    QueueHandle_t         queue_    = nullptr;
};

}  // namespace forward

#endif /* UDP_MAVLINK_FORWARDER_H */
