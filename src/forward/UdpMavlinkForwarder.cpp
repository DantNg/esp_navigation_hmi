#include "forward/UdpMavlinkForwarder.h"

#include <Arduino.h>

namespace forward {

namespace {
constexpr int kQueueDepth = 24;
}

void UdpMavlinkForwarder::configure(const char* ip, uint16_t port) {
    target_.fromString(ip);
    port_ = port;
}

void UdpMavlinkForwarder::start() {
    if (queue_) return;
    queue_ = xQueueCreate(kQueueDepth, sizeof(Item));
    xTaskCreatePinnedToCore(taskTrampoline, "fwd", 4096, this, 2, nullptr, 0);
}

void UdpMavlinkForwarder::onFrame(const telemetry::MavlinkFrame& frame) {
    if (!enabled_.load() || !queue_) return;

    Item item;
    item.len = frame.len;
    memcpy(item.data, frame.raw, frame.len);
    /* Drop (don't block the link hot-path) if the queue is full. */
    xQueueSend(queue_, &item, 0);
}

bool UdpMavlinkForwarder::send(const uint8_t* data, size_t len) {
    if (!wifi_.connected()) return false;
    if (udp_.beginPacket(target_, port_) != 1) return false;
    udp_.write(data, len);
    return udp_.endPacket() == 1;
}

void UdpMavlinkForwarder::taskTrampoline(void* arg) {
    static_cast<UdpMavlinkForwarder*>(arg)->run();
}

void UdpMavlinkForwarder::run() {
    Item item;
    for (;;) {
        if (xQueueReceive(queue_, &item, portMAX_DELAY) == pdTRUE) {
            if (send(item.data, item.len)) {
                forwarded_.fetch_add(1);
            }
        }
    }
}

}  // namespace forward
