#include "ui/MapView.h"

namespace ui {

namespace {
constexpr int      kMarkerSize  = 18;
constexpr uint32_t kMarkerColor = 0xff3b30;
}

void MapView::build(lv_obj_t* panel, gmap::MapExchange& exchange) {
    panel_    = panel;
    exchange_ = &exchange;
    /* Force layout so the panel's coords are valid before we read its size. */
    lv_obj_update_layout(panel_);
    panelW_   = lv_obj_get_width(panel_);
    panelH_   = lv_obj_get_height(panel_);

    /* The panel itself receives the pan gesture. */
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(panel_, panEvent, LV_EVENT_PRESSED,    this);
    lv_obj_add_event_cb(panel_, panEvent, LV_EVENT_PRESSING,   this);
    lv_obj_add_event_cb(panel_, panEvent, LV_EVENT_RELEASED,   this);
    lv_obj_add_event_cb(panel_, panEvent, LV_EVENT_PRESS_LOST, this);

    /* Map image (positioned at a negative offset; clipped by the panel). */
    dsc_.header.always_zero = 0;
    dsc_.header.w  = exchange_->width();
    dsc_.header.h  = exchange_->height();
    dsc_.header.cf = LV_IMG_CF_TRUE_COLOR;
    dsc_.data_size = (uint32_t)exchange_->width() * exchange_->height() * 2;
    dsc_.data      = exchange_->frontBuffer();

    img_ = lv_img_create(panel_);
    lv_img_set_src(img_, &dsc_);
    lv_obj_clear_flag(img_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(img_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(img_, LV_OBJ_FLAG_HIDDEN);  /* until first map arrives */

    /* Triangle marker, created after the image so it stays on top. */
    marker_ = lv_obj_create(panel_);
    lv_obj_set_size(marker_, kMarkerSize, kMarkerSize);
    lv_obj_set_style_bg_opa(marker_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(marker_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(marker_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(marker_, 0, 0);
    lv_obj_clear_flag(marker_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(marker_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(marker_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(marker_, markerDraw, LV_EVENT_DRAW_MAIN_END, nullptr);
}

void MapView::update(const telemetry::GeoPosition& pos) {
    if (!exchange_) return;

    /* (1) New map ready? Swap and recenter. */
    if (exchange_->trySwap()) {
        dsc_.data = exchange_->frontBuffer();
        lv_img_set_src(img_, &dsc_);
        lv_obj_invalidate(img_);
        if (!hasImage_) {
            lv_obj_clear_flag(img_, LV_OBJ_FLAG_HIDDEN);
            hasImage_ = true;
        }
        followDrone_ = true;  /* server centered the new map on the drone */
    }

    /* (2) Project the drone position onto the active map. */
    const gmap::MapProjection proj = exchange_->active();
    if (proj.valid() && pos.valid) {
        proj.toPixel(pos.lat, pos.lon, markerX_, markerY_);
        if (!markerShown_) {
            lv_obj_clear_flag(marker_, LV_OBJ_FLAG_HIDDEN);
            markerShown_ = true;
        }
        if (followDrone_) {
            viewX_ = markerX_ - panelW_ / 2;
            viewY_ = markerY_ - panelH_ / 2;
            clampView();
        }
    }

    reposition();
}

void MapView::notifyPanelResized() {
    if (!panel_) return;
    lv_obj_update_layout(panel_);
    panelW_ = lv_obj_get_width(panel_);
    panelH_ = lv_obj_get_height(panel_);
    followDrone_ = true;
    clampView();
    reposition();
}

void MapView::clampView() {
    const int32_t maxX = (int32_t)exchange_->width()  - panelW_;
    const int32_t maxY = (int32_t)exchange_->height() - panelH_;
    if (viewX_ < 0) viewX_ = 0;
    if (viewY_ < 0) viewY_ = 0;
    if (maxX > 0 && viewX_ > maxX) viewX_ = maxX;
    if (maxY > 0 && viewY_ > maxY) viewY_ = maxY;
}

void MapView::reposition() {
    if (hasImage_) {
        lv_obj_set_pos(img_, (lv_coord_t)(-viewX_), (lv_coord_t)(-viewY_));
    }
    if (markerShown_) {
        const lv_coord_t mx = (lv_coord_t)(markerX_ - viewX_ - kMarkerSize / 2);
        const lv_coord_t my = (lv_coord_t)(markerY_ - viewY_ - kMarkerSize / 2);
        lv_obj_set_pos(marker_, mx, my);
    }
}

void MapView::panEvent(lv_event_t* e) {
    auto* self = static_cast<MapView*>(lv_event_get_user_data(e));
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    if (code == LV_EVENT_PRESSED) {
        self->touchLastX_ = pt.x;
        self->touchLastY_ = pt.y;
        self->touching_   = true;
    } else if (code == LV_EVENT_PRESSING && self->touching_) {
        const int32_t dx = self->touchLastX_ - pt.x;
        const int32_t dy = self->touchLastY_ - pt.y;
        if (dx == 0 && dy == 0) return;
        self->touchLastX_ = pt.x;
        self->touchLastY_ = pt.y;
        self->viewX_ += dx;
        self->viewY_ += dy;
        self->followDrone_ = false;  /* user took manual control */
        self->clampView();
        self->reposition();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        self->touching_ = false;
    }
}

void MapView::markerDraw(lv_event_t* e) {
    lv_obj_t* obj           = lv_event_get_target(e);
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(kMarkerColor);
    dsc.bg_opa   = LV_OPA_COVER;

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const lv_point_t pts[3] = {
        {(lv_coord_t)((a.x1 + a.x2) / 2), a.y1},
        {a.x1, a.y2},
        {a.x2, a.y2},
    };
    lv_draw_polygon(draw_ctx, &dsc, pts, 3);
}

}  // namespace ui
