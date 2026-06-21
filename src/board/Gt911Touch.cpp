#include "board/Gt911Touch.h"

#include <Arduino.h>
#include <TAMC_GT911.h>

#include "board/ActiveBoard.h"

namespace board {

namespace {
constexpr auto& kT = kBoard.touch;
constexpr auto& kP = kBoard.panel;

/* Constructed from board config. With INT not wired (intr == -1) the GT911
 * factory config is left untouched and we only read the coordinate registers,
 * matching the original CrowPanel behaviour. */
TAMC_GT911 g_ts(kT.sda, kT.scl, kT.intr, kT.rst,
                kP.width  > 0 ? kP.width  : 1,
                kP.height > 0 ? kP.height : 1);
}  // namespace

bool Gt911Touch::begin() {
    Wire.begin(kT.sda, kT.scl);
    Wire.setClock(kT.i2cClock);
    g_ts.begin();
    g_ts.setRotation(ROTATION_NORMAL);
    return true;
}

bool Gt911Touch::read(int16_t& x, int16_t& y) {
    g_ts.read();
    if (!g_ts.isTouched) return false;

    const int rawX = kT.swapXy ? g_ts.points[0].y : g_ts.points[0].x;
    const int rawY = kT.swapXy ? g_ts.points[0].x : g_ts.points[0].y;

    x = (int16_t)map(rawX, kT.mapX1, kT.mapX2, 0, kP.width  - 1);
    y = (int16_t)map(rawY, kT.mapY1, kT.mapY2, 0, kP.height - 1);
    return true;
}

}  // namespace board
