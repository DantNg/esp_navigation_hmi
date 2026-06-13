#include "ui/SettingsView.h"

#include <WiFi.h>
#include <cstring>

#include "ui/gen/UiGen.h"

namespace ui {

using namespace gen;

/* -------------------------------------------------------------------------- */
void SettingsView::build(lv_obj_t* screen) {
    /* Full-screen dimmed overlay */
    cont_ = lv_obj_create(screen);
    lv_obj_set_size(cont_, kScreenW, kScreenH);
    lv_obj_set_pos(cont_, 0, 0);
    lv_obj_set_style_bg_color(cont_, lv_color_hex(kColBg), 0);
    lv_obj_set_style_bg_opa(cont_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont_, 0, 0);
    lv_obj_set_style_radius(cont_, 0, 0);
    lv_obj_set_style_pad_all(cont_, 16, 0);
    lv_obj_clear_flag(cont_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont_, LV_OBJ_FLAG_HIDDEN);

    /* Title */
    lv_obj_t* title = lv_label_create(cont_);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kColText), 0);
    lv_obj_set_pos(title, 4, 4);

    /* Close button */
    lv_obj_t* btnClose = lv_btn_create(cont_);
    lv_obj_set_size(btnClose, 56, 40);
    lv_obj_set_pos(btnClose, kScreenW - 32 - 56, 0);
    lv_obj_set_style_bg_color(btnClose, lv_color_hex(kColChip), 0);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, [](lv_event_t* e) {
        static_cast<SettingsView*>(lv_event_get_user_data(e))->close();
    }, LV_EVENT_CLICKED, this);

    const lv_coord_t rowY  = 56;
    const lv_coord_t rowH  = 44;
    const lv_coord_t labW  = 110;
    const lv_coord_t fieldW = 380;

    /* Networks dropdown + scan button */
    lv_obj_t* lblNet = lv_label_create(cont_);
    lv_label_set_text(lblNet, "Networks");
    lv_obj_set_style_text_color(lblNet, lv_color_hex(kColMuted), 0);
    lv_obj_set_pos(lblNet, 4, rowY + 12);

    ddSsid_ = lv_dropdown_create(cont_);
    lv_obj_set_size(ddSsid_, fieldW, rowH);
    lv_obj_set_pos(ddSsid_, labW, rowY);
    lv_dropdown_set_options(ddSsid_, "(scanning...)");
    lv_obj_add_event_cb(ddSsid_, [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        char buf[33];
        lv_dropdown_get_selected_str(self->ddSsid_, buf, sizeof(buf));
        if (buf[0] != '\0' && buf[0] != '(')
            lv_textarea_set_text(self->taSsid_, buf);
    }, LV_EVENT_VALUE_CHANGED, this);

    btnScan_ = lv_btn_create(cont_);
    lv_obj_set_size(btnScan_, 110, rowH);
    lv_obj_set_pos(btnScan_, labW + fieldW + 12, rowY);
    lv_obj_set_style_bg_color(btnScan_, lv_color_hex(kColChip), 0);
    lv_obj_t* lblScan = lv_label_create(btnScan_);
    lv_label_set_text(lblScan, LV_SYMBOL_REFRESH " Scan");
    lv_obj_center(lblScan);
    lv_obj_add_event_cb(btnScan_, [](lv_event_t* e) {
        static_cast<SettingsView*>(lv_event_get_user_data(e))->startScan();
    }, LV_EVENT_CLICKED, this);

    /* SSID textarea (editable, pre-filled by the dropdown) */
    lv_obj_t* lblSsid = lv_label_create(cont_);
    lv_label_set_text(lblSsid, "SSID");
    lv_obj_set_style_text_color(lblSsid, lv_color_hex(kColMuted), 0);
    lv_obj_set_pos(lblSsid, 4, rowY + rowH + 12 + 12);

    taSsid_ = lv_textarea_create(cont_);
    lv_textarea_set_one_line(taSsid_, true);
    lv_textarea_set_max_length(taSsid_, 32);
    lv_obj_set_size(taSsid_, fieldW, rowH);
    lv_obj_set_pos(taSsid_, labW, rowY + rowH + 12);

    /* Password textarea */
    lv_obj_t* lblPass = lv_label_create(cont_);
    lv_label_set_text(lblPass, "Password");
    lv_obj_set_style_text_color(lblPass, lv_color_hex(kColMuted), 0);
    lv_obj_set_pos(lblPass, 4, rowY + 2 * (rowH + 12) + 12);

    taPass_ = lv_textarea_create(cont_);
    lv_textarea_set_one_line(taPass_, true);
    lv_textarea_set_max_length(taPass_, 64);
    lv_textarea_set_password_mode(taPass_, true);
    lv_obj_set_size(taPass_, fieldW, rowH);
    lv_obj_set_pos(taPass_, labW, rowY + 2 * (rowH + 12));

    /* Show/hide password toggle */
    lv_obj_t* btnEye = lv_btn_create(cont_);
    lv_obj_set_size(btnEye, 56, rowH);
    lv_obj_set_pos(btnEye, labW + fieldW + 12, rowY + 2 * (rowH + 12));
    lv_obj_set_style_bg_color(btnEye, lv_color_hex(kColChip), 0);
    lv_obj_t* lblEye = lv_label_create(btnEye);
    lv_label_set_text(lblEye, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(lblEye);
    lv_obj_add_event_cb(btnEye, [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        const bool hide = !lv_textarea_get_password_mode(self->taPass_);
        lv_textarea_set_password_mode(self->taPass_, hide);
    }, LV_EVENT_CLICKED, this);

    /* Connect button */
    lv_obj_t* btnConnect = lv_btn_create(cont_);
    lv_obj_set_size(btnConnect, 178, rowH);
    lv_obj_set_pos(btnConnect, labW + fieldW + 12 + 56 + 12, rowY + 2 * (rowH + 12));
    lv_obj_set_style_bg_color(btnConnect, lv_color_hex(kColAccent), 0);
    lv_obj_set_style_text_color(btnConnect, lv_color_hex(kColBg), 0);
    lv_obj_t* lblGo = lv_label_create(btnConnect);
    lv_label_set_text(lblGo, LV_SYMBOL_OK " Connect");
    lv_obj_center(lblGo);
    lv_obj_add_event_cb(btnConnect, [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        const char* ssid = lv_textarea_get_text(self->taSsid_);
        const char* pass = lv_textarea_get_text(self->taPass_);
        if (ssid[0] == '\0') {
            self->setStatus("enter an SSID first", false);
            return;
        }
        lv_obj_add_flag(self->kb_, LV_OBJ_FLAG_HIDDEN);
        self->setStatus("connecting...", false);
        if (self->onConnect) self->onConnect(ssid, pass);
    }, LV_EVENT_CLICKED, this);

    /* Status line */
    lblStatus_ = lv_label_create(cont_);
    lv_label_set_text(lblStatus_, "");
    lv_obj_set_style_text_color(lblStatus_, lv_color_hex(kColMuted), 0);
    lv_obj_set_pos(lblStatus_, 4, rowY + 3 * (rowH + 12) + 4);

    /* Keyboard (bottom half, hidden until a textarea is focused) */
    kb_ = lv_keyboard_create(cont_);
    lv_obj_set_size(kb_, kScreenW - 32, 200);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb_, [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->kb_, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_READY, this);
    lv_obj_add_event_cb(kb_, [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->kb_, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CANCEL, this);

    auto focusCb = [](lv_event_t* e) {
        auto* self = static_cast<SettingsView*>(lv_event_get_user_data(e));
        self->showKeyboard(lv_event_get_target(e));
    };
    lv_obj_add_event_cb(taSsid_, focusCb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(taPass_, focusCb, LV_EVENT_FOCUSED, this);
}

/* -------------------------------------------------------------------------- */
void SettingsView::showKeyboard(lv_obj_t* ta) {
    lv_keyboard_set_textarea(kb_, ta);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_);
}

void SettingsView::open() {
    lv_obj_clear_flag(cont_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(cont_);
    startScan();
}

void SettingsView::close() {
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cont_, LV_OBJ_FLAG_HIDDEN);
}

bool SettingsView::isOpen() const {
    return cont_ && !lv_obj_has_flag(cont_, LV_OBJ_FLAG_HIDDEN);
}

/* -------------------------------------------------------------------------- */
void SettingsView::startScan() {
    if (scanning_) return;
    /* Async scan: works in STA and AP+STA modes, polled from tick(). */
    WiFi.scanNetworks(/*async=*/true);
    scanning_ = true;
    lv_dropdown_set_options(ddSsid_, "(scanning...)");
}

void SettingsView::tick() {
    if (!scanning_) return;
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    scanning_ = false;

    if (n <= 0) {
        lv_dropdown_set_options(ddSsid_, "(no networks found)");
        WiFi.scanDelete();
        return;
    }

    /* Newline-separated option list, deduplicated, capped to keep RAM small. */
    String opts;
    const int16_t kMaxShown = 15;
    for (int16_t i = 0; i < n && i < kMaxShown; i++) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        if (("\n" + opts + "\n").indexOf("\n" + ssid + "\n") >= 0) continue;
        if (opts.length()) opts += "\n";
        opts += ssid;
    }
    WiFi.scanDelete();
    lv_dropdown_set_options(ddSsid_, opts.length() ? opts.c_str()
                                                   : "(no networks found)");
}

/* -------------------------------------------------------------------------- */
void SettingsView::setStatus(const char* text, bool good) {
    if (!lblStatus_) return;
    lv_label_set_text(lblStatus_, text);
    lv_obj_set_style_text_color(lblStatus_,
        lv_color_hex(good ? kColGood : kColMuted), 0);
}

void SettingsView::setSsid(const char* ssid) {
    if (taSsid_ && ssid) lv_textarea_set_text(taSsid_, ssid);
}

}  // namespace ui
