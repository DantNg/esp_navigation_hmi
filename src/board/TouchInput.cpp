#include "board/TouchInput.h"

#include <lvgl.h>

#include "board/ActiveBoard.h"
#include "board/Gt911Touch.h"

namespace board {

namespace {

Gt911Touch     g_touch;
lv_indev_drv_t g_indev_drv;

void read_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
    int16_t x, y;
    if (g_touch.read(x, y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

}  // namespace

bool TouchInput::begin() {
    /* Both supported boards use GT911; branch here when another controller
     * family is added (see Gt911Touch.h). */
    static_assert(kBoard.touch.type == TouchType::GT911,
                  "active board's touch controller has no driver wired up");
    g_touch.begin();

    lv_indev_drv_init(&g_indev_drv);
    g_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    g_indev_drv.read_cb = read_cb;
    lv_indev_drv_register(&g_indev_drv);
    return true;
}

}  // namespace board
