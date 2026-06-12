/**
 * @file MapView.h
 * @brief Renders the active satellite map + drone marker into the map panel.
 *
 * Lives entirely on the UI task. Each refresh it (1) swaps in a newly fetched
 * map if MapExchange has one ready, (2) projects the drone position to a marker
 * pixel, (3) keeps the viewport centered on the drone (until the user pans by
 * touch). The map image is larger than the 560x480 panel, so we show it at a
 * negative offset and let the panel clip — exactly the manual-pan approach the
 * original tile viewer used, minus the SD tiles.
 */
#ifndef UI_MAP_VIEW_H
#define UI_MAP_VIEW_H

#include <lvgl.h>

#include "map/MapExchange.h"
#include "telemetry/TelemetryTypes.h"

namespace ui {

class MapView {
public:
    /** Create the image + marker inside `panel` and bind the exchange. */
    void build(lv_obj_t* panel, gmap::MapExchange& exchange);

    /** Per-refresh update from the UI task. */
    void update(const telemetry::GeoPosition& pos);

    /** Call after the map panel is resized (e.g. HUD↔map swap) so viewport
     *  dimensions stay in sync and the drone stays centred. */
    void notifyPanelResized();

private:
    static void panEvent(lv_event_t* e);
    static void markerDraw(lv_event_t* e);
    void reposition();
    void clampView();

    gmap::MapExchange* exchange_ = nullptr;
    lv_obj_t*    panel_  = nullptr;
    lv_obj_t*    img_    = nullptr;
    lv_obj_t*    marker_ = nullptr;
    lv_img_dsc_t dsc_{};

    int32_t viewX_   = 0;
    int32_t viewY_   = 0;
    int32_t markerX_ = 0;   /* drone pixel within map */
    int32_t markerY_ = 0;
    bool    hasImage_   = false;
    bool    markerShown_ = false;
    bool    followDrone_ = true;

    int32_t panelW_ = 0;
    int32_t panelH_ = 0;

    /* touch pan */
    int32_t touchLastX_ = 0;
    int32_t touchLastY_ = 0;
    bool    touching_   = false;
};

}  // namespace ui

#endif /* UI_MAP_VIEW_H */
