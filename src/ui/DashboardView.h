/**
 * @file DashboardView.h
 * @brief Full-screen HUD with map thumbnail — tapping the thumbnail swaps them.
 *
 * Layout:
 *   Primary panel  : full 800×480, initially the HUD (artificial horizon).
 *   Secondary panel: 220×148 thumbnail in the top-right corner, initially the map.
 *   Top overlay bar : 800×50  — mode, armed, link, wifi (semi-transparent).
 *   Bottom overlay  : 800×90  — battery, GPS, position, warning, switches.
 *   Click zone      : transparent, covers the thumbnail; tap to swap primary/secondary.
 *
 * All methods must be called from the UI task.
 */
#ifndef UI_DASHBOARD_VIEW_H
#define UI_DASHBOARD_VIEW_H

#include <lvgl.h>

#include <functional>

#include "board/BoardPins.h"
#include "telemetry/TelemetryTypes.h"
#include "ui/HudView.h"
#include "ui/gen/UiGen.h"

namespace ui {

class DashboardView {
public:
    void build(lv_obj_t* screen);
    void update(const telemetry::TelemetrySnapshot& snap);

    /** Panel that MapView draws the satellite image + marker into. */
    lv_obj_t* mapContainer() const { return w_.mapPanel; }

    /* ---- Callbacks wired by GroundStationApp ---- */
    std::function<void(bool /*enabled*/)> onForwardToggle;
    std::function<void(bool /*useUsb*/)>  onSourceToggle;
    /** Called after primary/secondary panels are resized (UiTask notifies MapView). */
    std::function<void()>                 onViewSwap;

    void setForwardState(bool enabled);
    void setSourceIsUsb(bool useUsb);
    void setWifiStatus(bool connected, const char* ip);

private:
    void applyLayout();
    void swapView();

    bool hudPrimary_ = true;

    /* All static layout + event wiring come from ui/ui_schema.json (UiGen.h). */
    gen::Widgets  w_;
    gen::Handlers h_;

    /* ---- HUD (custom-drawn, attached to w_.hudPanel) ---- */
    HudView   hud_;
};

}  // namespace ui

#endif /* UI_DASHBOARD_VIEW_H */
