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

namespace ui {

class DashboardView {
public:
    void build(lv_obj_t* screen);
    void update(const telemetry::TelemetrySnapshot& snap);

    /** Panel that MapView draws the satellite image + marker into. */
    lv_obj_t* mapContainer() const { return mapPanel_; }

    /* ---- Callbacks wired by GroundStationApp ---- */
    std::function<void(bool /*enabled*/)> onForwardToggle;
    std::function<void(bool /*useUsb*/)>  onSourceToggle;
    /** Called after primary/secondary panels are resized (UiTask notifies MapView). */
    std::function<void()>                 onViewSwap;

    void setForwardState(bool enabled);
    void setSourceIsUsb(bool useUsb);
    void setWifiStatus(bool connected, const char* ip);

private:
    static void forwardSwEvent(lv_event_t* e);
    static void sourceSwEvent(lv_event_t* e);
    static void thumbClickEvent(lv_event_t* e);

    void applyLayout();
    void swapView();

    /* ---- Geometry constants ---- */
    static constexpr lv_coord_t kTopBarH  = 50;
    static constexpr lv_coord_t kBotBarH  = 90;
    static constexpr lv_coord_t kThumbW   = 220;
    static constexpr lv_coord_t kThumbH   = 148;
    static constexpr lv_coord_t kThumbX   = board::kScreenWidth  - kThumbW - 4;  /* 576 */
    static constexpr lv_coord_t kThumbY   = kTopBarH + 2;                        /*  52 */

    bool hudPrimary_ = true;

    /* ---- Main panels ---- */
    lv_obj_t* hudPanel_    = nullptr;
    lv_obj_t* mapPanel_    = nullptr;

    /* ---- Overlay containers ---- */
    lv_obj_t* topBar_      = nullptr;
    lv_obj_t* botBar_      = nullptr;
    lv_obj_t* thumbClick_  = nullptr;   /* transparent tap zone over thumbnail */

    /* ---- HUD ---- */
    HudView   hud_;

    /* ---- Top-bar labels ---- */
    lv_obj_t* lblMode_     = nullptr;
    lv_obj_t* lblArmed_    = nullptr;
    lv_obj_t* lblLink_     = nullptr;
    lv_obj_t* lblWifi_     = nullptr;

    /* ---- Bottom-bar widgets ---- */
    lv_obj_t* barBatt_     = nullptr;
    lv_obj_t* lblBatt_     = nullptr;
    lv_obj_t* lblGps_      = nullptr;
    lv_obj_t* lblPos_      = nullptr;
    lv_obj_t* lblWarn_     = nullptr;
    lv_obj_t* swForward_   = nullptr;
    lv_obj_t* swSource_    = nullptr;
    lv_obj_t* lblSource_   = nullptr;   /* dynamic "UART"/"USB " text */
};

}  // namespace ui

#endif /* UI_DASHBOARD_VIEW_H */
