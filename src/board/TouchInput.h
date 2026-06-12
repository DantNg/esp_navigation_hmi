/**
 * @file TouchInput.h
 * @brief Facade that wires the GT911 touch panel into an LVGL input device.
 *
 * Single responsibility: own the LVGL indev registration. The actual GT911
 * register access stays in include/touch.h (board vendor code), included once
 * from TouchInput.cpp.
 */
#ifndef BOARD_TOUCH_INPUT_H
#define BOARD_TOUCH_INPUT_H

namespace board {

class TouchInput {
public:
    /** Init the GT911 controller and register the LVGL pointer indev. */
    bool begin();
};

}  // namespace board

#endif /* BOARD_TOUCH_INPUT_H */
