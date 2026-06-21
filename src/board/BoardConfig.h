/**
 * @file BoardConfig.h
 * @brief Hardware description contract for a display + touch board.
 *
 * This is the abstraction every board is expressed against (Dependency
 * Inversion): the rest of the firmware depends on `board::kBoard` — a compile-
 * time `BoardConfig` — and never on a specific GPIO number. To support a new
 * panel you add one header under board/boards/ that fills a `constexpr
 * BoardConfig`, then select it in ActiveBoard.h via a build flag. No existing
 * code changes (Open/Closed).
 *
 * Everything here is a plain value type so the whole description folds to
 * constants at compile time (zero RAM, no runtime branching on the target).
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <cstdint>

namespace board {

/** Parallel RGB565 panel: data lines, sync lines and timing.
 *  Data pins are named by colour bit; the display driver maps them to the
 *  underlying bus order (LovyanGFX d0..d15 = B0..B4, G0..G5, R0..R4). */
struct RgbPanel {
    uint16_t width;
    uint16_t height;

    /* RGB565 data pins (GPIO numbers) */
    int b0, b1, b2, b3, b4;
    int g0, g1, g2, g3, g4, g5;
    int r0, r1, r2, r3, r4;

    /* Control lines */
    int de;       /* data-enable / h-enable */
    int vsync;
    int hsync;
    int pclk;

    uint32_t pclkHz;

    /* Sync timing (porch / pulse / polarity) */
    int hsyncPolarity, hsyncFrontPorch, hsyncPulseWidth, hsyncBackPorch;
    int vsyncPolarity, vsyncFrontPorch, vsyncPulseWidth, vsyncBackPorch;
    int pclkActiveNeg, deIdleHigh, pclkIdleHigh;

    /* Backlight */
    int  backlight;
    bool backlightActiveHigh;

    /* Some parallel-RGB panels deliver photometric-negative colour (black<->white,
     * red<->cyan). Parallel RGB has no MCU invert command, so when true the flush
     * inverts each RGB565 pixel in software (see Display.cpp). */
    bool invert;
};

/** Supported touch controller families. */
enum class TouchType { None, GT911, FT6X36, XPT2046 };

/** Capacitive/resistive touch panel wiring + raw->screen coordinate mapping.
 *  The map fields feed Arduino map(): raw axis [mapX1..mapX2] -> [0..width-1].
 *  Use them to flip an axis (e.g. X1=width, X2=0 inverts X). */
struct TouchPanel {
    TouchType type;
    int      sda, scl, intr, rst;   /* intr/rst = -1 when not wired */
    uint32_t i2cClock;
    int      mapX1, mapX2, mapY1, mapY2;
    bool     swapXy;
};

/** SD card over SPI. */
struct SdCard { int cs, mosi, sck, miso; };

/** Telemetry UART (HardwareSerial). */
struct Telemetry { int rx, tx, uartNo; uint32_t baud; };

/** Full board description: one constexpr per supported board. */
struct BoardConfig {
    const char* name;
    RgbPanel    panel;
    TouchPanel  touch;
    SdCard      sd;
    Telemetry   telem;
};

}  // namespace board

#endif /* BOARD_CONFIG_H */
