#include "board/Display.h"

#include <Arduino.h>

#include "board/BoardPins.h"
#include "board/Lgfx.h"

/* The one and only panel instance, at GLOBAL scope to match the `extern LGFX
 * lcd;` in Lgfx.h and the global `lcd` used by include/touch.h. */
LGFX lcd;

namespace board {

namespace {

/* ~1/10 screen partial-render buffer in internal DRAM (fast, DMA-capable). */
constexpr uint32_t kBufLines = kScreenHeight / 10;
lv_disp_draw_buf_t g_draw_buf;
lv_color_t         g_buf[kScreenWidth * kBufLines];
lv_disp_drv_t      g_disp_drv;

void flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    const uint32_t w = (area->x2 - area->x1 + 1);
    const uint32_t h = (area->y2 - area->y1 + 1);

    /* Panels that ship photometric-negative colour: invert each RGB565 pixel
     * before pushing. Compile-time gated, so boards without the flag pay nothing.
     * Done before the DMA push so the buffer is final when the transfer starts. */
    if constexpr (kBoard.panel.invert) {
        const uint32_t n = w * h;
        for (uint32_t i = 0; i < n; i++) color_p[i].full ^= 0xFFFF;
    }

    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

}  // namespace

bool Display::begin() {
    lcd.begin();
    lcd.fillScreen(TFT_BLACK);
    delay(200);

    lv_init();

    width_  = lcd.width();
    height_ = lcd.height();

    lv_disp_draw_buf_init(&g_draw_buf, g_buf, nullptr, kScreenWidth * kBufLines);

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res  = width_;
    g_disp_drv.ver_res  = height_;
    g_disp_drv.flush_cb = flush_cb;
    g_disp_drv.draw_buf = &g_draw_buf;
    lv_disp_drv_register(&g_disp_drv);

    pinMode(kPinBacklight, OUTPUT);
    digitalWrite(kPinBacklight, HIGH);

    return true;
}

}  // namespace board
