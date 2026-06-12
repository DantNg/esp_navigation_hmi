#include "board/TouchInput.h"

#include <Arduino.h>
#include <lvgl.h>

/* Lgfx.h must come before touch.h: the GT911 coordinate scaler in touch.h
 * references the global `lcd` (lcd.width()/height()). */
#include "board/Lgfx.h"
#include "touch.h"

namespace board {

namespace {

lv_indev_drv_t g_indev_drv;

void read_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
    if (touch_has_signal() && touch_touched()) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

}  // namespace

bool TouchInput::begin() {
    touch_init();

    lv_indev_drv_init(&g_indev_drv);
    g_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    g_indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&g_indev_drv);
    return true;
}

}  // namespace board
