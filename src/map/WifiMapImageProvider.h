/**
 * @file WifiMapImageProvider.h
 * @brief Fetches a satellite map image (RGB565) from the PC map_server tool.
 *
 * Protocol (see tools/map_server.py):
 *   GET http://<host>:<port>/map?lat=<f>&lon=<f>&w=<w>&h=<h>&zoom=<z>
 *   Response body = 40-byte binary header followed by w*h*2 RGB565 LE bytes:
 *     magic[4]="MAP1", u16 w, u16 h,
 *     f64 latTop, f64 lonLeft, f64 latBottom, f64 lonRight
 */
#ifndef WIFI_MAP_IMAGE_PROVIDER_H
#define WIFI_MAP_IMAGE_PROVIDER_H

#include "map/IMapImageProvider.h"
#include "net/WifiManager.h"

namespace gmap {

class WifiMapImageProvider : public IMapImageProvider {
public:
    explicit WifiMapImageProvider(net::WifiManager& wifi) : wifi_(wifi) {}

    /** Set the PC tool endpoint. `host` must outlive this object. */
    void configure(const char* host, uint16_t port) {
        host_ = host;
        port_ = port;
    }

    bool fetch(double lat, double lon, uint8_t zoom, MapImage& out) override;

private:
    net::WifiManager& wifi_;
    const char*       host_ = nullptr;
    uint16_t          port_ = 8080;
};

}  // namespace gmap

#endif /* WIFI_MAP_IMAGE_PROVIDER_H */
