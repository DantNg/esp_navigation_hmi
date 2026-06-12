#include "ui/DashboardView.h"

#include <cstdio>

#include "board/BoardPins.h"
#include "telemetry/FlightMode.h"

namespace ui {

using telemetry::TelemetrySnapshot;
using telemetry::Severity;

namespace {
/* Palette */
constexpr uint32_t kColBg     = 0x0d1117;
constexpr uint32_t kColMap    = 0x05080d;
constexpr uint32_t kColText   = 0xe6edf3;
constexpr uint32_t kColMuted  = 0x8b95a3;
constexpr uint32_t kColAccent = 0x2dd4bf;
constexpr uint32_t kColGood   = 0x2ea043;
constexpr uint32_t kColBad    = 0xda3633;
constexpr uint32_t kColWarn   = 0xd29922;
constexpr uint32_t kColOverlay = 0x000000;

lv_obj_t* mkLabel(lv_obj_t* parent, const char* txt, const lv_font_t* font,
                  uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

/* Transparent, borderless, non-scrollable flex-row container inside parent. */
lv_obj_t* mkFlexRow(lv_obj_t* parent) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return r;
}

/* Invisible spacer that eats remaining flex space. */
lv_obj_t* mkSpacer(lv_obj_t* parent) {
    lv_obj_t* s = lv_obj_create(parent);
    lv_obj_set_height(s, 1);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_flex_grow(s, 1);
    return s;
}
}  // namespace

/* -------------------------------------------------------------------------- */
void DashboardView::build(lv_obj_t* screen) {
    lv_obj_set_style_bg_color(screen, lv_color_hex(kColBg), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- HUD panel (primary by default) ---- */
    hudPanel_ = lv_obj_create(screen);
    lv_obj_set_style_bg_color(hudPanel_, lv_color_hex(kColBg), 0);
    lv_obj_set_style_bg_opa(hudPanel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hudPanel_, 0, 0);
    lv_obj_set_style_radius(hudPanel_, 0, 0);
    lv_obj_set_style_pad_all(hudPanel_, 0, 0);
    lv_obj_clear_flag(hudPanel_, LV_OBJ_FLAG_SCROLLABLE);
    hud_.build(hudPanel_, board::kScreenWidth, board::kScreenHeight);

    /* ---- Map panel (thumbnail by default) ---- */
    mapPanel_ = lv_obj_create(screen);
    lv_obj_set_style_bg_color(mapPanel_, lv_color_hex(kColMap), 0);
    lv_obj_set_style_bg_opa(mapPanel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(mapPanel_, lv_color_hex(kColAccent), 0);
    lv_obj_set_style_border_width(mapPanel_, 2, 0);
    lv_obj_set_style_radius(mapPanel_, 4, 0);
    lv_obj_set_style_pad_all(mapPanel_, 0, 0);
    lv_obj_clear_flag(mapPanel_, LV_OBJ_FLAG_SCROLLABLE);
    /* Hint label — hidden once the first tile arrives */
    lv_obj_t* hint = mkLabel(mapPanel_, "MAP\nwaiting for GPS...",
                              &lv_font_montserrat_14, kColMuted);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(hint);

    /* ================================================================
     * TOP BAR — semi-transparent strip at y=0
     * ================================================================ */
    topBar_ = lv_obj_create(screen);
    lv_obj_set_size(topBar_, board::kScreenWidth, kTopBarH);
    lv_obj_set_pos(topBar_, 0, 0);
    lv_obj_set_style_bg_color(topBar_, lv_color_hex(kColOverlay), 0);
    lv_obj_set_style_bg_opa(topBar_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(topBar_, 0, 0);
    lv_obj_set_style_radius(topBar_, 0, 0);
    lv_obj_set_style_pad_hor(topBar_, 10, 0);
    lv_obj_set_style_pad_ver(topBar_, 6, 0);
    lv_obj_clear_flag(topBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(topBar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar_, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lblMode_ = mkLabel(topBar_, "---", &lv_font_montserrat_20, kColText);

    lblArmed_ = mkLabel(topBar_, " DISARMED ", &lv_font_montserrat_12, kColText);
    lv_obj_set_style_bg_color(lblArmed_, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_bg_opa(lblArmed_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lblArmed_, 4, 0);
    lv_obj_set_style_pad_hor(lblArmed_, 4, 0);
    lv_obj_set_style_pad_ver(lblArmed_, 2, 0);
    lv_obj_set_style_pad_left(lblArmed_, 8, 0);

    mkSpacer(topBar_);

    lblLink_ = mkLabel(topBar_, "link: —", &lv_font_montserrat_12, kColMuted);
    lv_obj_set_style_pad_right(lblLink_, 12, 0);

    lblWifi_ = mkLabel(topBar_, "wifi: off", &lv_font_montserrat_12, kColMuted);

    /* ================================================================
     * BOTTOM BAR — semi-transparent strip at bottom
     * ================================================================ */
    botBar_ = lv_obj_create(screen);
    lv_obj_set_size(botBar_, board::kScreenWidth, kBotBarH);
    lv_obj_set_pos(botBar_, 0, board::kScreenHeight - kBotBarH);
    lv_obj_set_style_bg_color(botBar_, lv_color_hex(kColOverlay), 0);
    lv_obj_set_style_bg_opa(botBar_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(botBar_, 0, 0);
    lv_obj_set_style_radius(botBar_, 0, 0);
    lv_obj_set_style_pad_all(botBar_, 6, 0);
    lv_obj_set_style_pad_row(botBar_, 3, 0);
    lv_obj_clear_flag(botBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(botBar_, LV_FLEX_FLOW_COLUMN);

    /* Row 1: battery bar + value | GPS */
    {
        lv_obj_t* row = mkFlexRow(botBar_);

        barBatt_ = lv_bar_create(row);
        lv_obj_set_size(barBatt_, 100, 8);
        lv_bar_set_range(barBatt_, 0, 100);
        lv_bar_set_value(barBatt_, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(barBatt_, lv_color_hex(0x30363d), LV_PART_MAIN);
        lv_obj_set_style_bg_color(barBatt_, lv_color_hex(kColGood), LV_PART_INDICATOR);

        lblBatt_ = mkLabel(row, " —", &lv_font_montserrat_12, kColText);
        lv_obj_set_style_pad_left(lblBatt_, 4, 0);
        lv_obj_set_flex_grow(lblBatt_, 1);

        lblGps_ = mkLabel(row, "GPS: —", &lv_font_montserrat_12, kColMuted);
    }

    /* Row 2: position */
    lblPos_ = mkLabel(botBar_, "no position", &lv_font_montserrat_12, kColMuted);

    /* Row 3: warning + controls */
    {
        lv_obj_t* row = mkFlexRow(botBar_);

        lblWarn_ = mkLabel(row, "", &lv_font_montserrat_12, kColMuted);
        lv_label_set_long_mode(lblWarn_, LV_LABEL_LONG_CLIP);
        lv_obj_set_flex_grow(lblWarn_, 1);

        /* WiFi-forward switch */
        {
            lv_obj_t* l = mkLabel(row, "FWD", &lv_font_montserrat_12, kColText);
            lv_obj_set_style_pad_left(l, 10, 0);
        }
        swForward_ = lv_switch_create(row);
        lv_obj_add_event_cb(swForward_, DashboardView::forwardSwEvent,
                            LV_EVENT_VALUE_CHANGED, this);
        lv_obj_set_style_pad_left(swForward_, 3, 0);

        /* Source switch + dynamic label */
        lblSource_ = mkLabel(row, "UART", &lv_font_montserrat_12, kColText);
        lv_obj_set_style_pad_left(lblSource_, 12, 0);
        swSource_ = lv_switch_create(row);
        lv_obj_add_event_cb(swSource_, DashboardView::sourceSwEvent,
                            LV_EVENT_VALUE_CHANGED, this);
        lv_obj_set_style_pad_left(swSource_, 3, 0);
    }

    /* ================================================================
     * THUMBNAIL CLICK ZONE — transparent, on top, covers the thumbnail
     * ================================================================ */
    thumbClick_ = lv_obj_create(screen);
    lv_obj_set_size(thumbClick_, kThumbW + 4, kThumbH + 4);
    lv_obj_set_style_bg_opa(thumbClick_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(thumbClick_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(thumbClick_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(thumbClick_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(thumbClick_, DashboardView::thumbClickEvent,
                        LV_EVENT_CLICKED, this);

    applyLayout();
}

/* -------------------------------------------------------------------------- */
void DashboardView::applyLayout() {
    lv_obj_t* primary = hudPrimary_ ? hudPanel_ : mapPanel_;
    lv_obj_t* thumb   = hudPrimary_ ? mapPanel_ : hudPanel_;

    /* Primary: full screen */
    lv_obj_set_size(primary, board::kScreenWidth, board::kScreenHeight);
    lv_obj_set_pos(primary, 0, 0);

    /* Thumbnail: fixed corner */
    lv_obj_set_size(thumb, kThumbW, kThumbH);
    lv_obj_set_pos(thumb, kThumbX, kThumbY);

    /* Position the click zone over the thumbnail */
    lv_obj_set_pos(thumbClick_, kThumbX - 2, kThumbY - 2);

    /* Z-order: primary at back, then thumbnail, then overlay bars, then click zone */
    lv_obj_move_background(primary);
    /* thumbnail stays naturally above primary in child list after this */
    lv_obj_move_foreground(topBar_);
    lv_obj_move_foreground(botBar_);
    lv_obj_move_foreground(thumbClick_);
}

/* -------------------------------------------------------------------------- */
void DashboardView::swapView() {
    hudPrimary_ = !hudPrimary_;
    applyLayout();
    if (onViewSwap) onViewSwap();
}

/* -------------------------------------------------------------------------- */
void DashboardView::update(const TelemetrySnapshot& s) {
    char buf[96];
    char scratch[24];

    /* Link */
    snprintf(buf, sizeof(buf), "link: %s %s · %lu fr",
             s.link.sourceName, s.link.linkUp ? "UP" : "—",
             (unsigned long)s.link.framesReceived);
    lv_label_set_text(lblLink_, buf);
    lv_obj_set_style_text_color(lblLink_,
        lv_color_hex(s.link.linkUp ? kColGood : kColMuted), 0);

    /* Mode + armed */
    lv_label_set_text(lblMode_,
        telemetry::flightModeName(s.mode, scratch, sizeof(scratch)));
    if (s.mode.armed) {
        lv_label_set_text(lblArmed_, " ARMED ");
        lv_obj_set_style_bg_color(lblArmed_, lv_color_hex(kColBad), 0);
    } else {
        lv_label_set_text(lblArmed_, " DISARMED ");
        lv_obj_set_style_bg_color(lblArmed_, lv_color_hex(0x30363d), 0);
    }

    /* HUD */
    hud_.update(s);

    /* Battery */
    if (s.battery.remaining >= 0) {
        snprintf(buf, sizeof(buf), "%.2fV %.1fA %d%%",
                 s.battery.voltage, s.battery.current, s.battery.remaining);
        lv_bar_set_value(barBatt_, s.battery.remaining, LV_ANIM_OFF);
        uint32_t c = s.battery.remaining > 50 ? kColGood
                   : s.battery.remaining > 20 ? kColWarn : kColBad;
        lv_obj_set_style_bg_color(barBatt_, lv_color_hex(c), LV_PART_INDICATOR);
    } else {
        snprintf(buf, sizeof(buf), "%.2fV %.1fA", s.battery.voltage, s.battery.current);
    }
    lv_label_set_text(lblBatt_, buf);

    /* GPS */
    const char* fix = s.gps.fixType >= 3 ? "3D"
                    : s.gps.fixType == 2 ? "2D" : "no fix";
    snprintf(buf, sizeof(buf), "GPS:%s %usats hdop%.1f",
             fix, s.gps.satellites, s.gps.hdop);
    lv_label_set_text(lblGps_, buf);

    /* Position */
    if (s.position.valid) {
        snprintf(buf, sizeof(buf), "%.6f, %.6f  alt:%.1fm  hdg:%.0f°",
                 s.position.lat, s.position.lon,
                 s.position.altRel, s.position.headingDeg);
    } else {
        snprintf(buf, sizeof(buf), "no position");
    }
    lv_label_set_text(lblPos_, buf);

    /* Warning */
    if (s.status.valid) {
        lv_label_set_text(lblWarn_, s.status.text);
        uint32_t c = (s.status.severity <= Severity::Error)   ? kColBad
                   : (s.status.severity <= Severity::Warning) ? kColWarn
                                                              : kColMuted;
        lv_obj_set_style_text_color(lblWarn_, lv_color_hex(c), 0);
    }
}

/* -------------------------------------------------------------------------- */
void DashboardView::setForwardState(bool enabled) {
    if (!swForward_) return;
    if (enabled) lv_obj_add_state(swForward_, LV_STATE_CHECKED);
    else         lv_obj_clear_state(swForward_, LV_STATE_CHECKED);
}

void DashboardView::setSourceIsUsb(bool useUsb) {
    if (!swSource_) return;
    if (useUsb) lv_obj_add_state(swSource_, LV_STATE_CHECKED);
    else        lv_obj_clear_state(swSource_, LV_STATE_CHECKED);
    if (lblSource_) lv_label_set_text(lblSource_, useUsb ? "USB " : "UART");
}

void DashboardView::setWifiStatus(bool connected, const char* ip) {
    if (!lblWifi_) return;
    char buf[48];
    if (connected) {
        snprintf(buf, sizeof(buf), "wifi: %s", ip ? ip : "connected");
    } else {
        snprintf(buf, sizeof(buf), "wifi: off");
    }
    lv_label_set_text(lblWifi_, buf);
    lv_obj_set_style_text_color(lblWifi_,
        lv_color_hex(connected ? kColGood : kColMuted), 0);
}

/* -------------------------------------------------------------------------- */
void DashboardView::forwardSwEvent(lv_event_t* e) {
    auto* self = static_cast<DashboardView*>(lv_event_get_user_data(e));
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (self->onForwardToggle) self->onForwardToggle(on);
}

void DashboardView::sourceSwEvent(lv_event_t* e) {
    auto* self = static_cast<DashboardView*>(lv_event_get_user_data(e));
    bool useUsb = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (self->lblSource_)
        lv_label_set_text(self->lblSource_, useUsb ? "USB " : "UART");
    if (self->onSourceToggle) self->onSourceToggle(useUsb);
}

void DashboardView::thumbClickEvent(lv_event_t* e) {
    static_cast<DashboardView*>(lv_event_get_user_data(e))->swapView();
}

}  // namespace ui
