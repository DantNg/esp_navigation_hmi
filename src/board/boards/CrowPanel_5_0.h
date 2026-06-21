/**
 * @file CrowPanel_5_0.h
 * @brief Board description for the Elecrow CrowPanel ESP32-S3 5.0" (800x480).
 *
 * Values lifted verbatim from the original Lgfx.h / BoardPins.h / touch.h so
 * behaviour is unchanged. Selected by default (no build flag) — see ActiveBoard.h.
 */
#ifndef BOARD_BOARDS_CROWPANEL_5_0_H
#define BOARD_BOARDS_CROWPANEL_5_0_H

#include "board/BoardConfig.h"

namespace board {
namespace boards {

inline constexpr BoardConfig kCrowPanel50 = {
    /* name  */ "Elecrow CrowPanel ESP32-S3 5.0\"",
    /* panel */ {
        800, 480,
        /* B0..B4 */  8,  3, 46,  9,  1,
        /* G0..G5 */  5,  6,  7, 15, 16,  4,
        /* R0..R4 */ 45, 48, 47, 21, 14,
        /* de,vsync,hsync,pclk */ 40, 41, 39, 0,
        /* pclkHz */ 15000000,
        /* hsync: pol,fp,pw,bp */ 0, 8, 4, 43,
        /* vsync: pol,fp,pw,bp */ 0, 8, 4, 12,
        /* pclkActiveNeg, deIdleHigh, pclkIdleHigh */ 1, 0, 0,
        /* backlight, activeHigh */ 2, true,
        /* invert */ false,
    },
    /* touch */ {
        TouchType::GT911,
        /* sda,scl,intr,rst */ 19, 20, -1, -1,
        /* i2cClock */ 400000,
        /* mapX1,X2,Y1,Y2 (both axes inverted) */ 800, 0, 480, 0,
        /* swapXy */ false,
    },
    /* sd    */ { /*cs*/ 10, /*mosi*/ 11, /*sck*/ 12, /*miso*/ 13 },
    /* telem */ { /*rx*/ 44, /*tx*/ 43, /*uartNo*/ 1, /*baud*/ 57600 },
};

}  // namespace boards
}  // namespace board

#endif /* BOARD_BOARDS_CROWPANEL_5_0_H */
