/**
 * @file Gt911Touch.h
 * @brief GT911 capacitive touch driver, parametrized by the active board.
 *
 * Single responsibility: read a touch point from the GT911 and map raw
 * coordinates to screen pixels using board::kBoard.touch. It depends only on
 * the BoardConfig abstraction — no hard-coded pins, no global display object —
 * so a new GT911 board needs zero changes here (just different config data).
 *
 * To support a different controller family (FT6X36, XPT2046) add a sibling
 * driver implementing the same read()/begin() shape and branch on
 * board::kBoard.touch.type in TouchInput.
 */
#ifndef BOARD_GT911_TOUCH_H
#define BOARD_GT911_TOUCH_H

#include <cstdint>

namespace board {

class Gt911Touch {
public:
    /** Init I2C + controller from the active board's touch config. */
    bool begin();

    /** Poll once. Returns true and fills x/y (screen pixels) when touched. */
    bool read(int16_t& x, int16_t& y);
};

}  // namespace board

#endif /* BOARD_GT911_TOUCH_H */
