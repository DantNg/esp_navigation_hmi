#include "map/WifiMapImageProvider.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <cstring>

namespace gmap {

namespace {
constexpr int      kHeaderSize   = 40;
constexpr uint32_t kHttpTimeout  = 8000;

bool readFully(Stream* s, uint8_t* buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        size_t want = len - got;
        if (want > 4096) want = 4096;
        size_t n = s->readBytes(buf + got, want);
        if (n == 0) return false;  /* inter-byte timeout */
        got += n;
    }
    return true;
}

double rdF64(const uint8_t* p) {  /* little-endian -> native (ESP32 is LE) */
    double d;
    memcpy(&d, p, sizeof(d));
    return d;
}
}  // namespace

bool WifiMapImageProvider::fetch(double lat, double lon, uint8_t zoom, MapImage& out) {
    if (!wifi_.connected() || !out.data || !host_) return false;

    char url[160];
    snprintf(url, sizeof(url),
             "http://%s:%u/map?lat=%.7f&lon=%.7f&w=%u&h=%u&zoom=%u",
             host_, port_, lat, lon, out.w, out.h, zoom);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(kHttpTimeout);
    if (!http.begin(client, url)) {
        Serial.println("[MAP] http.begin failed");
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[MAP] GET %s -> %d\n", url, code);
        http.end();
        return false;
    }

    Stream* s = http.getStreamPtr();
    s->setTimeout(kHttpTimeout);

    uint8_t header[kHeaderSize];
    bool ok = readFully(s, header, kHeaderSize);
    if (ok && memcmp(header, "MAP1", 4) != 0) {
        Serial.println("[MAP] bad magic in response");
        ok = false;
    }

    uint16_t rw = 0, rh = 0;
    if (ok) {
        rw = header[4] | (header[5] << 8);
        rh = header[6] | (header[7] << 8);
        if (rw != out.w || rh != out.h) {
            Serial.printf("[MAP] size mismatch: got %ux%u want %ux%u\n",
                          rw, rh, out.w, out.h);
            ok = false;
        }
    }

    if (ok) {
        const size_t pixBytes = (size_t)out.w * out.h * 2;
        ok = readFully(s, out.data, pixBytes);
        if (!ok) Serial.println("[MAP] pixel read timeout");
    }

    if (ok) {
        out.bounds.latTop    = rdF64(header + 8);
        out.bounds.lonLeft   = rdF64(header + 16);
        out.bounds.latBottom = rdF64(header + 24);
        out.bounds.lonRight  = rdF64(header + 32);
        Serial.printf("[MAP] fetched %ux%u @ lat %.6f lon %.6f\n", rw, rh, lat, lon);
    }

    http.end();
    return ok;
}

}  // namespace gmap
