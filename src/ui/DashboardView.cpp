#include "ui/DashboardView.h"

#include <cstdio>

#include "board/BoardPins.h"
#include "telemetry/FlightMode.h"

namespace ui {

using telemetry::TelemetrySnapshot;
using telemetry::Severity;

/* Static layout/palette live in ui/ui_schema.json -> generated ui/gen/UiGen.h. */
using namespace gen;

/* -------------------------------------------------------------------------- */
void DashboardView::build(lv_obj_t* screen) {
    /* Whole static tree + event wiring come from the schema. */
    gen::build(screen, w_, &h_);

    /* Custom-drawn HUD attaches to the schema-defined panel. */
    hud_.build(w_.hudPanel, board::kScreenWidth, board::kScreenHeight);

    /* Handler bodies for the events declared in the schema. */
    h_.onForwardChanged = [this](lv_event_t* e) {
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        if (onForwardToggle) onForwardToggle(on);
    };
    h_.onSourceChanged = [this](lv_event_t* e) {
        bool useUsb = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        if (w_.lblSource) lv_label_set_text(w_.lblSource, useUsb ? "USB " : "UART");
        if (onSourceToggle) onSourceToggle(useUsb);
    };
    h_.onThumbClicked = [this](lv_event_t*) { swapView(); };
    h_.onSettingsClicked = [this](lv_event_t*) {
        if (onSettingsOpen) onSettingsOpen();
    };

    applyLayout();
}

/* -------------------------------------------------------------------------- */
void DashboardView::applyLayout() {
    lv_obj_t* primary = hudPrimary_ ? w_.hudPanel : w_.mapPanel;
    lv_obj_t* thumb   = hudPrimary_ ? w_.mapPanel : w_.hudPanel;

    /* Primary: full screen */
    lv_obj_set_size(primary, kScreenW, kScreenH);
    lv_obj_set_pos(primary, 0, 0);

    /* Thumbnail: fixed corner */
    lv_obj_set_size(thumb, kThumbW, kThumbH);
    lv_obj_set_pos(thumb, kThumbX, kThumbY);

    /* Position the click zone over the thumbnail */
    lv_obj_set_pos(w_.thumbClick, kThumbX - 2, kThumbY - 2);

    /* Z-order: primary at back, then thumbnail, then overlay bar, then click zone */
    lv_obj_move_background(primary);
    /* thumbnail stays naturally above primary in child list after this */
    lv_obj_move_foreground(w_.botBar);
    lv_obj_move_foreground(w_.thumbClick);
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
    lv_label_set_text(w_.lblLink, buf);
    lv_obj_set_style_text_color(w_.lblLink,
        lv_color_hex(s.link.linkUp ? kColGood : kColMuted), 0);

    /* Mode + armed */
    lv_label_set_text(w_.lblMode,
        telemetry::flightModeName(s.mode, scratch, sizeof(scratch)));
    if (s.mode.armed) {
        lv_label_set_text(w_.lblArmed, " ARMED ");
        lv_obj_set_style_bg_color(w_.lblArmed, lv_color_hex(kColBad), 0);
    } else {
        lv_label_set_text(w_.lblArmed, " DISARMED ");
        lv_obj_set_style_bg_color(w_.lblArmed, lv_color_hex(kColChip), 0);
    }

    /* HUD */
    hud_.update(s);

    /* Battery */
    if (s.battery.remaining >= 0) {
        snprintf(buf, sizeof(buf), "%.2fV %.1fA %d%%",
                 s.battery.voltage, s.battery.current, s.battery.remaining);
        lv_bar_set_value(w_.barBatt, s.battery.remaining, LV_ANIM_OFF);
        uint32_t c = s.battery.remaining > 50 ? kColGood
                   : s.battery.remaining > 20 ? kColWarn : kColBad;
        lv_obj_set_style_bg_color(w_.barBatt, lv_color_hex(c), LV_PART_INDICATOR);
    } else {
        snprintf(buf, sizeof(buf), "%.2fV %.1fA", s.battery.voltage, s.battery.current);
    }
    lv_label_set_text(w_.lblBatt, buf);

    /* GPS */
    const char* fix = s.gps.fixType >= 3 ? "3D"
                    : s.gps.fixType == 2 ? "2D" : "no fix";
    snprintf(buf, sizeof(buf), "GPS:%s %usats hdop%.1f",
             fix, s.gps.satellites, s.gps.hdop);
    lv_label_set_text(w_.lblGps, buf);

    /* Position */
    if (s.position.valid) {
        snprintf(buf, sizeof(buf), "%.6f, %.6f  alt:%.1fm  hdg:%.0f°",
                 s.position.lat, s.position.lon,
                 s.position.altRel, s.position.headingDeg);
    } else {
        snprintf(buf, sizeof(buf), "no position");
    }
    lv_label_set_text(w_.lblPos, buf);

    /* Warning */
    if (s.status.valid) {
        lv_label_set_text(w_.lblWarn, s.status.text);
        uint32_t c = (s.status.severity <= Severity::Error)   ? kColBad
                   : (s.status.severity <= Severity::Warning) ? kColWarn
                                                              : kColMuted;
        lv_obj_set_style_text_color(w_.lblWarn, lv_color_hex(c), 0);
    }
}

/* -------------------------------------------------------------------------- */
void DashboardView::setForwardState(bool enabled) {
    if (!w_.swForward) return;
    if (enabled) lv_obj_add_state(w_.swForward, LV_STATE_CHECKED);
    else         lv_obj_clear_state(w_.swForward, LV_STATE_CHECKED);
}

void DashboardView::setSourceIsUsb(bool useUsb) {
    if (!w_.swSource) return;
    if (useUsb) lv_obj_add_state(w_.swSource, LV_STATE_CHECKED);
    else        lv_obj_clear_state(w_.swSource, LV_STATE_CHECKED);
    if (w_.lblSource) lv_label_set_text(w_.lblSource, useUsb ? "USB " : "UART");
}

void DashboardView::setWifiStatus(bool connected, const char* ip) {
    if (!w_.lblWifi) return;
    char buf[80];
    snprintf(buf, sizeof(buf), "wifi: %s",
             ip && ip[0] ? ip : (connected ? "connected" : "off"));
    lv_label_set_text(w_.lblWifi, buf);
    lv_obj_set_style_text_color(w_.lblWifi,
        lv_color_hex(connected ? kColGood : kColMuted), 0);
}

}  // namespace ui
