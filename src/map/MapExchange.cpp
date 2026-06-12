#include "map/MapExchange.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace gmap {

MapExchange::MapExchange() { mutex_ = xSemaphoreCreateMutex(); }

MapExchange::~MapExchange() {
    if (mutex_) vSemaphoreDelete(mutex_);
    for (auto* b : buf_) if (b) heap_caps_free(b);
}

bool MapExchange::allocate(uint16_t width, uint16_t height) {
    w_ = width;
    h_ = height;
    const size_t bytes = (size_t)w_ * h_ * 2;
    for (int i = 0; i < 2; i++) {
        buf_[i] = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!buf_[i]) {
            Serial.printf("[MAP] PSRAM alloc failed for buffer %d (%u bytes)\n",
                          i, (unsigned)bytes);
            return false;
        }
        memset(buf_[i], 0, bytes);
    }
    Serial.printf("[MAP] Allocated 2x %ux%u RGB565 buffers (%.2f MB total) in PSRAM\n",
                  w_, h_, (2.0 * bytes) / (1024.0 * 1024.0));
    return true;
}

bool MapExchange::trySwap() {
    if (!ready_.load()) return false;
    const int back = 1 - frontIdx_.load();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    active_ = pendingProj_;
    xSemaphoreGive(mutex_);
    frontIdx_.store(back);  /* filled back buffer becomes the front */
    ready_.store(false);    /* release the back buffer to the producer */
    return true;
}

MapProjection MapExchange::active() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    MapProjection copy = active_;
    xSemaphoreGive(mutex_);
    return copy;
}

}  // namespace gmap
