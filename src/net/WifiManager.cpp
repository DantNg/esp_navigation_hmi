#include "net/WifiManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

namespace net {

constexpr char WifiManager::kApSsid[];
constexpr char WifiManager::kApPass[];

void WifiManager::begin(const char* ssid, const char* pass) {
    /* Clear any mode/credentials stored in NVS from a previous flash.
     * Without persistent(false), the ESP32 bootloader restores the last
     * saved mode (AP, AP+STA, …) which overrides the mode() call below. */
    WiFi.persistent(false);
    WiFi.disconnect(true);   /* disconnect + erase config from RAM */
    delay(100);

    strncpy(ssid_, ssid ? ssid : "", sizeof(ssid_) - 1);
    strncpy(pass_, pass ? pass : "", sizeof(pass_) - 1);

    /* Setup AP stays up until the station connects, so the board is always
     * reachable even with wrong/missing credentials. */
    startAp();
    WiFi.setSleep(false);
    WiFi.begin(ssid_, pass_);
    started_    = true;
    retryCount_ = 0;
    lastTryMs_  = millis();
    Serial.printf("[WIFI] AP \"%s\" up (%s), connecting to \"%s\"...\n",
                  kApSsid, apIp_, ssid_);
}

void WifiManager::setCredentials(const char* ssid, const char* pass) {
    strncpy(ssid_, ssid ? ssid : "", sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';
    strncpy(pass_, pass ? pass : "", sizeof(pass_) - 1);
    pass_[sizeof(pass_) - 1] = '\0';

    Serial.printf("[WIFI] new credentials, connecting to \"%s\"...\n", ssid_);
    if (!apActive_) startAp();   /* stay reachable while the new attempt runs */
    WiFi.disconnect();
    delay(50);
    WiFi.begin(ssid_, pass_);
    started_    = true;
    retryCount_ = 0;
    lastTryMs_  = millis();
}

void WifiManager::startAp() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(kApSsid, kApPass);
    strncpy(apIp_, WiFi.softAPIP().toString().c_str(), sizeof(apIp_) - 1);
    apIp_[sizeof(apIp_) - 1] = '\0';
    apActive_ = true;
}

void WifiManager::stopAp() {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive_ = false;
    Serial.println("[WIFI] STA connected — setup AP stopped");
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
        if (apActive_) stopAp();
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
            /* Station keeps failing — bring the setup AP back so the user can
             * fix the credentials, and restart the stack. */
            if (!apActive_) startAp();
            Serial.println("[WIFI] max retries — re-init (AP up)");
            WiFi.disconnect();
            delay(100);
            WiFi.begin(ssid_, pass_);
            retryCount_ = 0;
        }
    }
}

}  // namespace net
