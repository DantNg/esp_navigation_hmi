/**
 * @file MapExchange.h
 * @brief Lock-light handoff of a freshly fetched map from MapTask to UiTask.
 *
 * Two PSRAM buffers form a front/back pair. MapController (producer, core 0)
 * fills the back buffer over WiFi while UiTask (consumer, core 1) keeps drawing
 * the front buffer, then publishes. UiTask swaps the pointer — no tearing, and
 * LVGL is still only ever touched by UiTask.
 *
 * Handshake invariant: the producer only writes the back buffer while
 * ready()==false; the consumer only swaps while ready()==true. They alternate,
 * so the two never touch the same buffer at once.
 */
#ifndef MAP_EXCHANGE_H
#define MAP_EXCHANGE_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstdint>

#include "map/MapProjection.h"

namespace gmap {

class MapExchange {
public:
    MapExchange();
    ~MapExchange();

    /** Allocate both RGB565 buffers in PSRAM (w*h*2 each). */
    bool allocate(uint16_t width, uint16_t height);
    bool allocated() const { return buf_[0] && buf_[1]; }
    uint16_t width()  const { return w_; }
    uint16_t height() const { return h_; }

    /* ---- Producer (MapController / MapTask) ---- */
    bool     ready() const { return ready_.load(); }
    uint8_t* backBuffer() const { return buf_[1 - frontIdx_.load()]; }
    /** Mark the back buffer filled with the given projection; awaits a swap. */
    void publish(const MapProjection& proj) {
        pendingProj_ = proj;
        ready_.store(true);
    }

    /* ---- Consumer (UiTask) ---- */
    /** If a new map is pending, swap it to the front. Returns true on swap. */
    bool trySwap();
    uint8_t* frontBuffer() const { return buf_[frontIdx_.load()]; }

    /** Thread-safe copy of the currently displayed projection. */
    MapProjection active() const;

private:
    uint8_t*           buf_[2] = {nullptr, nullptr};
    uint16_t           w_ = 0;
    uint16_t           h_ = 0;
    std::atomic<int>   frontIdx_{0};
    std::atomic<bool>  ready_{false};
    MapProjection      pendingProj_;   /* valid while ready_ */

    mutable SemaphoreHandle_t mutex_;
    MapProjection             active_;
};

}  // namespace gmap

#endif /* MAP_EXCHANGE_H */
