#include "map/MapController.h"

#include <Arduino.h>

namespace gmap {

void MapController::start() {
    xTaskCreatePinnedToCore(taskTrampoline, "map", 16384, this, 2, nullptr, 0);
}

void MapController::taskTrampoline(void* arg) {
    static_cast<MapController*>(arg)->run();
}

bool MapController::needsNewMap(const MapProjection& active,
                                 double lat, double lon) const {
    if (!active.valid()) return true;
    int32_t px = 0, py = 0;
    active.toPixel(lat, lon, px, py);
    const int32_t m = edgeMargin_;
    return px < m || py < m ||
           px > (int32_t)active.width()  - m ||
           py > (int32_t)active.height() - m;
}

void MapController::run() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kTickMs));

        if (!exchange_.allocated()) continue;
        if (exchange_.ready())      continue;   /* awaiting UiTask swap */

        const telemetry::TelemetrySnapshot snap = store_.get();

        double lat, lon;
        if (snap.position.valid) {
            lat = snap.position.lat;
            lon = snap.position.lon;
        } else {
            /* No GPS yet — use provider's built-in default (e.g. SD map centre).
             * WiFi provider returns false here so we keep waiting for GPS. */
            if (!provider_.getDefaultPosition(lat, lon)) continue;
        }

        if (!needsNewMap(exchange_.active(), lat, lon)) continue;

        const uint32_t now = millis();
        if (now - lastFetchMs_ < kMinFetchMs) continue;
        lastFetchMs_ = now;

        MapImage img;
        img.data = exchange_.backBuffer();
        img.w    = exchange_.width();
        img.h    = exchange_.height();
        if (provider_.fetch(lat, lon, zoom_, img)) {
            exchange_.publish(MapProjection(img.bounds, img.w, img.h));
        }
    }
}

}  // namespace gmap
