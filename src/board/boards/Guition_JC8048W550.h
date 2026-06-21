/**
 * @file Guition_JC8048W550.h
 * @brief Board description for the GUITION JC8048W550 ESP32-S3 5.0" (800x480).
 *
 * Derived from JC8048W550_hardware_config.h. Same ST7262-class RGB panel and
 * GT911 touch as the CrowPanel; the differences are PCLK pin (42 vs 0), the
 * sync back-porches, PCLK frequency (14 MHz) and a wired touch reset (GPIO38).
 * Touch coordinates already arrive as panel pixels, so the map is identity.
 *
 * Select with build flag -D BOARD_GUITION_JC8048W550 (see ActiveBoard.h).
 *
 * NOTE: the source config does not document SD-card or telemetry-UART pins for
 * this board; the CrowPanel defaults are reused below. Verify against your unit
 * before relying on the SD map / telemetry features.
 */
#ifndef BOARD_BOARDS_GUITION_JC8048W550_H
#define BOARD_BOARDS_GUITION_JC8048W550_H

#include "board/BoardConfig.h"

namespace board {
namespace boards {

inline constexpr BoardConfig kGuitionJC8048W550 = {
    /* name  */ "GUITION JC8048W550 ESP32-S3 5.0\"",
    /* panel */ {
        800, 480,
        /* B0..B4 */  8,  3, 46,  9,  1,
        /* G0..G5 */  5,  6,  7, 15, 16,  4,
        /* R0..R4 */ 45, 48, 47, 21, 14,
        /* de,vsync,hsync,pclk */ 40, 41, 39, 42,
        /* pclkHz — TEMP 10 MHz diagnostic: low clock gives the framebuffer GDMA
         * more time per pixel; if noise clears, the cause is PSRAM bandwidth. */ 10000000,
        /* Larger back-porches give the framebuffer GDMA more blanking time to keep
         * up (LovyanGFX has no bounce buffer); the JC8048W550 config's tight bp=8
         * caused grain/darkening. These match the CrowPanel's known-good timing. */
        /* hsync: pol,fp,pw,bp */ 0, 8, 4, 43,
        /* vsync: pol,fp,pw,bp */ 0, 8, 4, 12,
        /* pclkActiveNeg, deIdleHigh, pclkIdleHigh */ 1, 0, 0,
        /* backlight, activeHigh */ 2, true,
        /* invert — same RGB wiring as the CrowPanel, colours are already correct */ false,
    },
    /* touch */ {
        TouchType::GT911,
        /* sda,scl,intr,rst */ 19, 20, -1, 38,
        /* i2cClock */ 400000,
        /* GT911 glass is mounted 90° rotated vs the display, so swap X/Y. Raw axes
         * run low->high (verified on hardware: bottom-left reads raw x≈800, y≈0).
         * After swap, screen-X comes from raw Y (0..480) and screen-Y from raw X
         * (0..800). mapX1,X2 (->screen X), mapY1,Y2 (->screen Y) */ 0, 480, 0, 800,
        /* swapXy */ true,
    },
    /* sd    */ { /*cs*/ 10, /*mosi*/ 11, /*sck*/ 12, /*miso*/ 13 },   /* unverified */
    /* telem */ { /*rx*/ 44, /*tx*/ 43, /*uartNo*/ 1, /*baud*/ 57600 }, /* unverified */
};

}  // namespace boards
}  // namespace board

#endif /* BOARD_BOARDS_GUITION_JC8048W550_H */
