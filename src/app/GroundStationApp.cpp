#include "app/GroundStationApp.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

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

    /* WiFi settings screen: persist new credentials, then re-associate. */
    ui_.settings().setSsid(config_.wifiSsid);
    ui_.settings().onConnect = [this](const char* ssid, const char* pass) {
        strncpy(config_.wifiSsid, ssid, sizeof(config_.wifiSsid) - 1);
        config_.wifiSsid[sizeof(config_.wifiSsid) - 1] = '\0';
        strncpy(config_.wifiPass, pass, sizeof(config_.wifiPass) - 1);
        config_.wifiPass[sizeof(config_.wifiPass) - 1] = '\0';
        config_.save();
        wifi_.setCredentials(config_.wifiSsid, config_.wifiPass);
    };

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
            const bool isUp = ws == WifiManager::Status::Connected;
            char buf[64];
            if (isUp) {
                snprintf(buf, sizeof(buf), "%s", wifi_.ip());
            } else if (wifi_.apActive()) {
                /* Setup AP is up — show how to reach the board. */
                snprintf(buf, sizeof(buf), "AP %s (%s)",
                         WifiManager::apSsid(), wifi_.apIp());
            } else {
                snprintf(buf, sizeof(buf), "%s",
                         ws == WifiManager::Status::Connecting ? "connecting..." :
                         ws == WifiManager::Status::Failed     ? "failed" : "off");
            }
            ui_.dashboard().setWifiStatus(isUp, buf);

            /* Keep the settings screen's status line current too. */
            if (isUp) {
                char sbuf[80];
                snprintf(sbuf, sizeof(sbuf), "connected to \"%s\" — %s",
                         wifi_.ssid(), wifi_.ip());
                ui_.settings().setStatus(sbuf, true);
            } else if (ws == WifiManager::Status::Connecting) {
                char sbuf[96];
                snprintf(sbuf, sizeof(sbuf),
                         "connecting to \"%s\"...  (AP: %s / %s)",
                         wifi_.ssid(), WifiManager::apSsid(), wifi_.apIp());
                ui_.settings().setStatus(sbuf, false);
            } else if (ws == WifiManager::Status::Failed) {
                char sbuf[96];
                snprintf(sbuf, sizeof(sbuf),
                         "connection failed  (AP: %s, pass 12345678, %s)",
                         WifiManager::apSsid(), wifi_.apIp());
                ui_.settings().setStatus(sbuf, false);
            }
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
