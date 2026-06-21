/**
 * @file BoardPins.h
 * @brief Convenience constants for the active board, derived from board::kBoard.
 *
 * These names are kept for the rest of the codebase; their values come from the
 * selected BoardConfig (see ActiveBoard.h), so switching boards never touches a
 * call site. The RGB panel data pins/timing live in the BoardConfig and are
 * consumed by Display/Lgfx; everything peripheral is surfaced here.
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <cstdint>

#include "board/ActiveBoard.h"

namespace board {

/* ---- Display ---- */
constexpr uint16_t kScreenWidth  = kBoard.panel.width;
constexpr uint16_t kScreenHeight = kBoard.panel.height;
constexpr int      kPinBacklight = kBoard.panel.backlight;

/* ---- Touch ---- */
constexpr int kPinTouchSda = kBoard.touch.sda;
constexpr int kPinTouchScl = kBoard.touch.scl;

/* ---- SD card (SPI) ---- */
constexpr int kPinSdCs   = kBoard.sd.cs;
constexpr int kPinSdMosi = kBoard.sd.mosi;
constexpr int kPinSdSck  = kBoard.sd.sck;
constexpr int kPinSdMiso = kBoard.sd.miso;

/* ---- Drone telemetry UART (HardwareSerial) ---- */
constexpr int      kPinTelemRx  = kBoard.telem.rx;
constexpr int      kPinTelemTx  = kBoard.telem.tx;
constexpr uint32_t kTelemBaud   = kBoard.telem.baud;
constexpr int      kTelemUartNo = kBoard.telem.uartNo;

}  // namespace board

#endif /* BOARD_PINS_H */
