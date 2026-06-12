/**
 * @file HudView.h
 * @brief Mission-Planner-style HUD: artificial horizon + tapes.
 *
 * A single custom-drawn LVGL object. update() stores the latest attitude/air
 * data and invalidates; the draw callback renders the sky/ground horizon
 * (rolled + pitched), a pitch ladder, a roll arc with a bank pointer, the fixed
 * aircraft symbol, and airspeed / altitude / heading readouts. UI task only.
 */
#ifndef UI_HUD_VIEW_H
#define UI_HUD_VIEW_H

#include <lvgl.h>

#include "telemetry/TelemetryTypes.h"

namespace ui {

class HudView {
public:
    /** Create the HUD object inside `parent` at the given size. */
    void build(lv_obj_t* parent, lv_coord_t width, lv_coord_t height);

    /** Push new telemetry and trigger a redraw. */
    void update(const telemetry::TelemetrySnapshot& snap);

private:
    static void drawEvent(lv_event_t* e);
    void draw(lv_event_t* e);

    lv_obj_t* obj_ = nullptr;

    float rollRad_   = 0;
    float pitchRad_  = 0;
    float headingDeg_ = 0;
    float altM_      = 0;
    float airspeed_  = 0;
    float climb_     = 0;
    bool  valid_     = false;
};

}  // namespace ui

#endif /* UI_HUD_VIEW_H */
