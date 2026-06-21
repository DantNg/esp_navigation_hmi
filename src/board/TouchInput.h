/**
 * @file TouchInput.h
 * @brief Facade that wires the GT911 touch panel into an LVGL input device.
 *
 * Single responsibility: own the LVGL indev registration. The actual GT911
 * register access + coordinate mapping live in board/Gt911Touch, which is
 * parametrized by the active board's BoardConfig (no hard-coded pins).
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
