/**
 * @file Lgfx.h
 * @brief LovyanGFX device for the active board's parallel RGB panel.
 *
 * The concrete pins/timing are NOT hard-coded here anymore: the constructor
 * reads them from board::kBoard.panel (BoardConfig). Supporting another RGB
 * panel is purely a data change in a board header — this file is unchanged.
 *
 * A single global `lcd` instance is shared by the display flush callback
 * (Display.cpp).
 */
#ifndef BOARD_LGFX_H
#define BOARD_LGFX_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include "board/ActiveBoard.h"

class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB   _bus_instance;
    lgfx::Panel_RGB _panel_instance;

    LGFX(void) {
        constexpr auto& p = board::kBoard.panel;
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            /* LovyanGFX bus order d0..d15 = B0..B4, G0..G5, R0..R4. */
            cfg.pin_d0  = p.b0;  cfg.pin_d1  = p.b1;  cfg.pin_d2  = p.b2;
            cfg.pin_d3  = p.b3;  cfg.pin_d4  = p.b4;
            cfg.pin_d5  = p.g0;  cfg.pin_d6  = p.g1;  cfg.pin_d7  = p.g2;
            cfg.pin_d8  = p.g3;  cfg.pin_d9  = p.g4;  cfg.pin_d10 = p.g5;
            cfg.pin_d11 = p.r0;  cfg.pin_d12 = p.r1;  cfg.pin_d13 = p.r2;
            cfg.pin_d14 = p.r3;  cfg.pin_d15 = p.r4;

            cfg.pin_henable = p.de;
            cfg.pin_vsync   = p.vsync;
            cfg.pin_hsync   = p.hsync;
            cfg.pin_pclk    = p.pclk;
            cfg.freq_write  = p.pclkHz;

            cfg.hsync_polarity    = p.hsyncPolarity;
            cfg.hsync_front_porch = p.hsyncFrontPorch;
            cfg.hsync_pulse_width = p.hsyncPulseWidth;
            cfg.hsync_back_porch  = p.hsyncBackPorch;

            cfg.vsync_polarity    = p.vsyncPolarity;
            cfg.vsync_front_porch = p.vsyncFrontPorch;
            cfg.vsync_pulse_width = p.vsyncPulseWidth;
            cfg.vsync_back_porch  = p.vsyncBackPorch;

            cfg.pclk_active_neg = p.pclkActiveNeg;
            cfg.de_idle_high    = p.deIdleHigh;
            cfg.pclk_idle_high  = p.pclkIdleHigh;

            _bus_instance.config(cfg);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = p.width;
            cfg.memory_height = p.height;
            cfg.panel_width   = p.width;
            cfg.panel_height  = p.height;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);
        setPanel(&_panel_instance);
    }
};

/* Single shared instance — defined in Display.cpp. */
extern LGFX lcd;

#endif /* BOARD_LGFX_H */
