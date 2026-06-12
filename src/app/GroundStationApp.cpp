#include "app/GroundStationApp.h"

#include <Arduino.h>

namespace app {

telemetry::ITelemetrySource* GroundStationApp::sourceFor(bool useUsb) {
    return useUsb ? static_cast<telemetry::ITelemetrySource*>(&usbSource_)
                  : static_cast<telemetry::ITelemetrySource*>(&uartSource_);
}

void GroundStationApp::begin() {
    Serial.begin(115200);
    /* USB CDC: wait up to 5 s for the host to open the port (DTR).
     * Without this, early Serial.println() bytes are silently dropped because
     * USBCDC::write() returns 0 when no host has the port open yet.
     * On boot without a PC attached the 5 s passes and execution continues. */
    {
        const uint32_t kWaitMs = 5000;
        uint32_t t0 = millis();
        while (!Serial && (millis() - t0) < kWaitMs) delay(50);
    }
    Serial.println("\n\n============================");
    Serial.println("[BOOT] Lite Ground Station");
    Serial.println("============================");
    Serial.flush();

    config_.load();

    display_.begin();
    touch_.begin();

    /* Allocate map buffers in PSRAM before the UI binds to them. */
    mapReady_ = mapExchange_.allocate(config_.mapW, config_.mapH);
    if (mapReady_) ui_.attachMap(mapExchange_);

    ui_.begin();

    /* WiFi + forward (always start WiFi for forward feature, even if SD map) */
    wifi_.begin(config_.wifiSsid, config_.wifiPass);
    forwarder_.configure(config_.gcsIp, config_.gcsPort);
    forwarder_.setEnabled(config_.forwardEnabled);
    forwarder_.start();

    /* Select map provider and start the map task */
    if (mapReady_) {
        if (config_.mapSource == 2) {
            Serial.println("[BOOT] map source: SD XYZ tiles (/tiles/Z/X/Y.bin)");
            xyzMapProvider_.setDefaultCenter(config_.defaultLat, config_.defaultLon);
            if (!xyzMapProvider_.begin()) {
                Serial.println("[BOOT] XYZ map provider init failed — map disabled");
                mapReady_ = false;
            } else {
                mapController_ = new gmap::MapController(store_, mapExchange_,
                                                          xyzMapProvider_);
            }
        } else if (config_.mapSource == 1) {
            Serial.println("[BOOT] map source: SD split tiles (/map/)");
            if (!sdMapProvider_.begin()) {
                Serial.println("[BOOT] SD map provider init failed — map disabled");
                mapReady_ = false;
            } else {
                mapController_ = new gmap::MapController(store_, mapExchange_,
                                                          sdMapProvider_);
            }
        } else {
            Serial.println("[BOOT] map source: WiFi");
            wifiMapProvider_.configure(config_.pcHost, config_.pcPort);
            mapController_ = new gmap::MapController(store_, mapExchange_,
                                                      wifiMapProvider_);
        }

        if (mapController_) {
            mapController_->configure(config_.mapZoom, config_.edgeMarginPx);
            mapController_->start();
        }
    }

    /* UI controls -> subsystems */
    ui_.dashboard().onForwardToggle = [this](bool on) { forwarder_.setEnabled(on); };
    ui_.dashboard().onSourceToggle  = [this](bool useUsb) {
        link_.requestSource(sourceFor(useUsb));
    };
    ui_.dashboard().setForwardState(config_.forwardEnabled);
    ui_.dashboard().setSourceIsUsb(config_.linkSourceUsb);

    /* Telemetry pipeline */
    uartSource_.setBaud(config_.telemBaud);
    link_.addConsumer(&decoder_);
    link_.addConsumer(&forwarder_);
    link_.setSource(sourceFor(config_.linkSourceUsb));

    xTaskCreatePinnedToCore(linkTaskTrampoline, "link", 4096, this, 3, nullptr, 0);

    Serial.println("[BOOT] setup done");
}

void GroundStationApp::loop() {
    ui_.tick();
    ota_.loop();

    const uint32_t now = millis();
    if (now - lastWifiMs_ >= 1000) {
        lastWifiMs_ = now;
        wifi_.loop();
        /* OTA/SD web server needs an IP — start it once WiFi is up. */
        if (wifi_.connected() && !ota_.started()) {
            ota_.begin();
            Serial.printf("[OTA] update page: http://%s/\n", wifi_.ip());
        }
        {
            using net::WifiManager;
            const WifiManager::Status ws = wifi_.status();
            const char* statusStr =
                ws == WifiManager::Status::Connected  ? wifi_.ip()       :
                ws == WifiManager::Status::Connecting ? "connecting..."   :
                ws == WifiManager::Status::Failed     ? "failed"          : "off";
            ui_.dashboard().setWifiStatus(ws == WifiManager::Status::Connected,
                                          statusStr);
        }
    }

    delay(5);
}

void GroundStationApp::linkTaskTrampoline(void* arg) {
    static_cast<GroundStationApp*>(arg)->runLinkTask();
}

void GroundStationApp::runLinkTask() {
    for (;;) {
        link_.poll();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

}  // namespace app
