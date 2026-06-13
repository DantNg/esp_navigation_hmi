#include "ui/UiTask.h"

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

namespace {
constexpr uint32_t kRefreshIntervalMs = 200;  /* 5 Hz sidebar refresh */
}

void UiTask::begin() {
    dashboard_.build(lv_scr_act());
    settings_.build(lv_scr_act());
    dashboard_.onSettingsOpen = [this]() { settings_.open(); };
    if (exchange_) {
        mapView_.build(dashboard_.mapContainer(), *exchange_);
        /* After a HUD↔map swap the map panel is resized — notify MapView. */
        dashboard_.onViewSwap = [this]() { mapView_.notifyPanelResized(); };
    }
}

void UiTask::tick() {
    lv_timer_handler();
    settings_.tick();   /* polls the async WiFi scan when the panel is open */

    const uint32_t now = millis();
    if (now - lastRefreshMs_ >= kRefreshIntervalMs) {
        lastRefreshMs_ = now;
        const telemetry::TelemetrySnapshot snap = store_.get();
        dashboard_.update(snap);
        if (exchange_) mapView_.update(snap.position);
    }
}

}  // namespace ui
