/**
 * @file UiTask.h
 * @brief The single LVGL-owning context (runs on the Arduino loop / core 1).
 *
 * LVGL is not thread-safe, so every LVGL call in the firmware happens here.
 * tick() pumps the LVGL timer handler and, on a throttle, copies a snapshot out
 * of the TelemetryStore into the DashboardView. Other subsystems (link, forward,
 * map) run on separate tasks and communicate only through thread-safe stores.
 */
#ifndef UI_TASK_H
#define UI_TASK_H

#include <cstdint>

#include "map/MapExchange.h"
#include "telemetry/TelemetryStore.h"
#include "ui/DashboardView.h"
#include "ui/MapView.h"

namespace ui {

class UiTask {
public:
    explicit UiTask(telemetry::TelemetryStore& store) : store_(store) {}

    /** Bind the satellite-map exchange (optional). Call before begin(). */
    void attachMap(gmap::MapExchange& exchange) { exchange_ = &exchange; }

    /** Build the screen. Call once after the display/touch are up. */
    void begin();

    /** Pump LVGL + refresh widgets. Call repeatedly from loop(). */
    void tick();

    DashboardView& dashboard() { return dashboard_; }

private:
    telemetry::TelemetryStore& store_;
    DashboardView              dashboard_;
    MapView                    mapView_;
    gmap::MapExchange*          exchange_ = nullptr;
    uint32_t                   lastRefreshMs_ = 0;
};

}  // namespace ui

#endif /* UI_TASK_H */
