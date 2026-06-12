/**
 * @file WifiManager.h
 * @brief Minimal WiFi station bring-up + status, decoupled from its consumers.
 */
#ifndef NET_WIFI_MANAGER_H
#define NET_WIFI_MANAGER_H

#include <cstdint>

namespace net {

class WifiManager {
public:
    enum class Status { Idle, Connecting, Connected, Failed };

    /** Start connecting (non-blocking) in station mode. */
    void begin(const char* ssid, const char* pass);

    /** True while associated + IP acquired. */
    bool connected() const;

    /** Current connection status (for UI display). */
    Status status() const;

    /** Dotted local IP (valid only while connected()). */
    const char* ip();

    /** Periodic upkeep: refresh cached IP, kick reconnect if dropped. */
    void loop();

private:
    char     ip_[16]        = "0.0.0.0";
    uint32_t lastTryMs_     = 0;
    uint8_t  retryCount_    = 0;
    bool     started_       = false;

    static constexpr uint32_t kRetryMs  = 8000;
    static constexpr uint8_t  kMaxRetry = 10;   /* show "failed" after 10 retries */
};

}  // namespace net

#endif /* NET_WIFI_MANAGER_H */
