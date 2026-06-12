/**
 * @file Display.h
 * @brief Thin facade that brings up the panel + LVGL display driver.
 *
 * Single responsibility: own the LVGL display registration and the panel
 * backlight. It knows nothing about telemetry or the app — callers just call
 * begin() once during startup. Touch is handled separately by TouchInput.
 */
#ifndef BOARD_DISPLAY_H
#define BOARD_DISPLAY_H

#include <lvgl.h>
#include <cstdint>

namespace board {

class Display {
public:
    /** Init LovyanGFX panel, backlight, and register the LVGL display driver. */
    bool begin();

    uint16_t width() const  { return width_; }
    uint16_t height() const { return height_; }

private:
    uint16_t width_  = 0;
    uint16_t height_ = 0;
};

}  // namespace board

#endif /* BOARD_DISPLAY_H */
