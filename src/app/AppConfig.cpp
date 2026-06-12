#include "app/AppConfig.h"

#include <Preferences.h>
#include <cstring>

namespace app {

namespace {
constexpr char kNs[] = "lgs";  /* NVS namespace */

void getStr(Preferences& p, const char* key, char* dst, size_t dstSize) {
    if (p.isKey(key)) {
        String v = p.getString(key, dst);
        strncpy(dst, v.c_str(), dstSize - 1);
        dst[dstSize - 1] = '\0';
    }
}
}  // namespace

void AppConfig::load() {
    Preferences p;
    if (!p.begin(kNs, /*readOnly=*/true)) return;

    getStr(p, "ssid", wifiSsid, sizeof(wifiSsid));
    getStr(p, "pass", wifiPass, sizeof(wifiPass));
    getStr(p, "pcHost", pcHost, sizeof(pcHost));
    getStr(p, "gcsIp", gcsIp, sizeof(gcsIp));

    pcPort        = p.getUShort("pcPort", pcPort);
    gcsPort       = p.getUShort("gcsPort", gcsPort);
    linkSourceUsb = p.getBool("usb", linkSourceUsb);
    telemBaud     = p.getULong("baud", telemBaud);
    forwardEnabled = p.getBool("fwd", forwardEnabled);
    mapW          = p.getUShort("mapW", mapW);
    mapH          = p.getUShort("mapH", mapH);
    mapZoom       = (uint8_t)p.getUChar("zoom", mapZoom);
    edgeMarginPx  = p.getUShort("edge", edgeMarginPx);
    mapSource     = (uint8_t)p.getUChar("mapsrc", mapSource);
    defaultLat    = p.getDouble("defLat", defaultLat);
    defaultLon    = p.getDouble("defLon", defaultLon);

    p.end();
}

void AppConfig::save() const {
    Preferences p;
    if (!p.begin(kNs, /*readOnly=*/false)) return;

    p.putString("ssid", wifiSsid);
    p.putString("pass", wifiPass);
    p.putString("pcHost", pcHost);
    p.putString("gcsIp", gcsIp);
    p.putUShort("pcPort", pcPort);
    p.putUShort("gcsPort", gcsPort);
    p.putBool("usb", linkSourceUsb);
    p.putULong("baud", telemBaud);
    p.putBool("fwd", forwardEnabled);
    p.putUShort("mapW", mapW);
    p.putUShort("mapH", mapH);
    p.putUChar("zoom", mapZoom);
    p.putUShort("edge", edgeMarginPx);
    p.putUChar("mapsrc", mapSource);
    p.putDouble("defLat", defaultLat);
    p.putDouble("defLon", defaultLon);

    p.end();
}

}  // namespace app
