/**
 * @file GroundStationApp.h
 * @brief Composition root: owns every subsystem and wires them together.
 */
#ifndef APP_GROUND_STATION_APP_H
#define APP_GROUND_STATION_APP_H

#include <cstdint>

#include "app/AppConfig.h"
#include "board/BoardPins.h"
#include "board/Display.h"
#include "board/TouchInput.h"
#include "forward/UdpMavlinkForwarder.h"
#include "map/MapController.h"
#include "map/MapExchange.h"
#include "map/SdTileMapProvider.h"
#include "map/SdXyzTileProvider.h"
#include "map/WifiMapImageProvider.h"
#include "net/OtaWebService.h"
#include "net/WifiManager.h"
#include "telemetry/LinkManager.h"
#include "telemetry/TelemetryDecoder.h"
#include "telemetry/TelemetryStore.h"
#include "telemetry/UartTelemetrySource.h"
#include "telemetry/UsbTelemetrySource.h"
#include "ui/UiTask.h"

namespace app {

class GroundStationApp {
public:
    void begin();
    void loop();

private:
    static void linkTaskTrampoline(void* arg);
    void runLinkTask();
    telemetry::ITelemetrySource* sourceFor(bool useUsb);

    /* Declaration order = construction order */
    AppConfig         config_;
    board::Display    display_;
    board::TouchInput touch_;
    net::WifiManager    wifi_;
    net::OtaWebService  ota_;

    telemetry::TelemetryStore     store_;
    gmap::MapExchange             mapExchange_;
    gmap::WifiMapImageProvider    wifiMapProvider_{wifi_};
    gmap::SdTileMapProvider       sdMapProvider_;
    gmap::SdXyzTileProvider       xyzMapProvider_;
    forward::UdpMavlinkForwarder  forwarder_{wifi_};
    telemetry::TelemetryDecoder   decoder_{store_};
    telemetry::UartTelemetrySource uartSource_{board::kTelemUartNo, board::kPinTelemRx,
                                               board::kPinTelemTx, board::kTelemBaud};
    telemetry::UsbTelemetrySource usbSource_;
    telemetry::LinkManager        link_{store_};
    /* MapController uses the provider selected by config_.mapSource */
    gmap::MapController*          mapController_ = nullptr;
    ui::UiTask                    ui_{store_};

    bool     mapReady_   = false;
    uint32_t lastWifiMs_ = 0;
};

}  // namespace app

#endif /* APP_GROUND_STATION_APP_H */
