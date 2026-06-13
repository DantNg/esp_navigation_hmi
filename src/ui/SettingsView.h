/**
 * @file SettingsView.h
 * @brief Full-screen WiFi settings overlay: scan networks, enter password.
 *
 * Hidden by default; opened from the dashboard's gear button. Scanning uses
 * the async WiFi scan API and is polled from tick(), so the UI never blocks.
 * All methods must be called from the UI task.
 */
#ifndef UI_SETTINGS_VIEW_H
#define UI_SETTINGS_VIEW_H

#include <lvgl.h>

#include <functional>

namespace ui {

class SettingsView {
public:
    /** Build the (hidden) overlay on `screen`. Call once from UiTask::begin(). */
    void build(lv_obj_t* screen);

    /** Show the overlay and start a network scan. */
    void open();
    void close();
    bool isOpen() const;

    /** Poll the async scan; call every UI tick. */
    void tick();

    /** Connection status line shown at the bottom (e.g. "connecting…"). */
    void setStatus(const char* text, bool good);

    /** Pre-fill the SSID box (current configured network). */
    void setSsid(const char* ssid);

    /** Called when the user taps CONNECT with a non-empty SSID. */
    std::function<void(const char* ssid, const char* pass)> onConnect;

private:
    void startScan();
    void showKeyboard(lv_obj_t* ta);

    lv_obj_t* cont_      = nullptr;   /* full-screen overlay root */
    lv_obj_t* ddSsid_    = nullptr;   /* scan results dropdown */
    lv_obj_t* taSsid_    = nullptr;
    lv_obj_t* taPass_    = nullptr;
    lv_obj_t* kb_        = nullptr;
    lv_obj_t* lblStatus_ = nullptr;
    lv_obj_t* btnScan_   = nullptr;

    bool scanning_ = false;
};

}  // namespace ui

#endif /* UI_SETTINGS_VIEW_H */
