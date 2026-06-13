/**
 * @file WifiManager.h
 * @brief WiFi station bring-up with AP fallback, decoupled from its consumers.
 *
 * Behaviour:
 *   - begin() starts in AP+STA mode: the setup AP ("GCS-Setup") is up while the
 *     station tries to associate, so the board is always reachable.
 *   - Once the station connects, the AP is shut down (pure STA).
 *   - If the station later fails for kMaxRetry retries, the AP comes back up.
 *   - setCredentials() switches to a new network at runtime (settings screen).
 */
#ifndef NET_WIFI_MANAGER_H
#define NET_WIFI_MANAGER_H

#include <cstdint>

namespace net {

class WifiManager {
public:
    enum class Status { Idle, Connecting, Connected, Failed };

    /** Start connecting (non-blocking): AP up + station association running. */
    void begin(const char* ssid, const char* pass);

    /** Drop the current association and connect to a new network.
     *  The setup AP is re-enabled while the new attempt runs. */
    void setCredentials(const char* ssid, const char* pass);

    /** True while associated + IP acquired. */
    bool connected() const;

    /** Current connection status (for UI display). */
    Status status() const;

    /** Dotted local IP (valid only while connected()). */
    const char* ip();

    /** True while the setup access point is broadcasting. */
    bool apActive() const { return apActive_; }

    /** AP IP (valid while apActive()), typically 192.168.4.1. */
    const char* apIp() const { return apIp_; }

    /** SSID of the setup AP. */
    static const char* apSsid() { return kApSsid; }

    /** SSID the station is currently configured for. */
    const char* ssid() const { return ssid_; }

    /** Periodic upkeep: refresh cached IP, kick reconnect, manage the AP. */
    void loop();

private:
    void startAp();
    void stopAp();

    char     ssid_[33]      = "";
    char     pass_[65]      = "";
    char     ip_[16]        = "0.0.0.0";
    char     apIp_[16]      = "0.0.0.0";
    uint32_t lastTryMs_     = 0;
    uint8_t  retryCount_    = 0;
    bool     started_       = false;
    bool     apActive_      = false;

    static constexpr char     kApSsid[] = "GCS-Setup";
    static constexpr char     kApPass[] = "12345678";
    static constexpr uint32_t kRetryMs  = 8000;
    static constexpr uint8_t  kMaxRetry = 10;   /* show "failed" after 10 retries */
};

}  // namespace net

#endif /* NET_WIFI_MANAGER_H */
