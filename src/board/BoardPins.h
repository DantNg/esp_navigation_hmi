/**
 * @file BoardPins.h
 * @brief Central hardware pin/constant map for the CrowPanel ESP32-S3 5.0".
 *
 * All board-specific wiring lives here so the rest of the code never hard-codes
 * a GPIO number. The RGB panel data pins are configured inside Display.cpp (they
 * belong to the LovyanGFX bus descriptor); everything else is defined here.
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <cstdint>

namespace board {

/* ---- Display ---- */
constexpr uint16_t kScreenWidth  = 800;
constexpr uint16_t kScreenHeight = 480;
constexpr int      kPinBacklight = 2;     /* TFT_BL */

/* ---- Touch (GT911 over I2C) — see include/touch.h ---- */
constexpr int kPinTouchSda = 19;
constexpr int kPinTouchScl = 20;

/* ---- SD card (SPI) ---- */
constexpr int kPinSdCs   = 10;
constexpr int kPinSdMosi = 11;
constexpr int kPinSdSck  = 12;
constexpr int kPinSdMiso = 13;

/* ---- Drone telemetry UART (HardwareSerial #1) ----
 * Default RX=44 / TX=43 match the serial header the original firmware used for
 * an external module. Confirm against your CrowPanel's exposed pins. */
constexpr int      kPinTelemRx  = 44;
constexpr int      kPinTelemTx  = 43;
constexpr uint32_t kTelemBaud   = 57600;   /* common ArduPilot/PX4 telem rate */
constexpr int      kTelemUartNo = 1;       /* Serial1 */

}  // namespace board

#endif /* BOARD_PINS_H */
