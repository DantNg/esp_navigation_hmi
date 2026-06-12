#include "net/WifiManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

namespace net {

void WifiManager::begin(const char* ssid, const char* pass) {
    /* Clear any mode/credentials stored in NVS from a previous flash.
     * Without persistent(false), the ESP32 bootloader restores the last
     * saved mode (AP, AP+STA, …) which overrides the mode(WIFI_STA) call. */
    WiFi.persistent(false);
    WiFi.disconnect(true);   /* disconnect + erase config from RAM */
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, pass);
    started_    = true;
    retryCount_ = 0;
    lastTryMs_  = millis();
    Serial.printf("[WIFI] STA mode, connecting to \"%s\"...\n", ssid);
}

bool WifiManager::connected() const {
    return started_ && WiFi.status() == WL_CONNECTED;
}

WifiManager::Status WifiManager::status() const {
    if (!started_)                      return Status::Idle;
    if (WiFi.status() == WL_CONNECTED)  return Status::Connected;
    if (retryCount_ >= kMaxRetry)       return Status::Failed;
    return Status::Connecting;
}

const char* WifiManager::ip() {
    if (connected()) {
        strncpy(ip_, WiFi.localIP().toString().c_str(), sizeof(ip_) - 1);
        ip_[sizeof(ip_) - 1] = '\0';
    }
    return ip_;
}

void WifiManager::loop() {
    if (!started_) return;
    if (WiFi.status() == WL_CONNECTED) {
        ip();           /* refresh cache */
        retryCount_ = 0;
        return;
    }
    const uint32_t now = millis();
    if (now - lastTryMs_ > kRetryMs) {
        lastTryMs_ = now;
        if (retryCount_ < kMaxRetry) {
            retryCount_++;
            Serial.printf("[WIFI] retry #%u\n", retryCount_);
            WiFi.reconnect();
        } else {
            /* Restart the stack once after max retries */
            Serial.println("[WIFI] max retries — re-init");
            WiFi.disconnect(true);
            delay(100);
            WiFi.reconnect();
            retryCount_ = 0;
        }
    }
}

}  // namespace net
