/**
 * @file MapController.h
 * @brief MapTask: keep the active map covering the drone, fetching on demand.
 *
 * Provider-agnostic — works with WifiMapImageProvider, SdTileMapProvider, or
 * any IMapImageProvider.  The provider itself decides whether it is ready
 * (WiFi connected, SD mounted, etc.); this controller just calls fetch().
 */
#ifndef MAP_CONTROLLER_H
#define MAP_CONTROLLER_H

#include <cstdint>

#include "map/IMapImageProvider.h"
#include "map/MapExchange.h"
#include "telemetry/TelemetryStore.h"

namespace gmap {

class MapController {
public:
    MapController(telemetry::TelemetryStore& store, MapExchange& exchange,
                  IMapImageProvider& provider)
        : store_(store), exchange_(exchange), provider_(provider) {}

    void configure(uint8_t zoom, uint16_t edgeMarginPx) {
        zoom_       = zoom;
        edgeMargin_ = edgeMarginPx;
    }

    /** Spawn the MapTask (core 0). */
    void start();

private:
    static void taskTrampoline(void* arg);
    void run();
    bool needsNewMap(const MapProjection& active, double lat, double lon) const;

    static constexpr uint32_t kTickMs     = 500;
    static constexpr uint32_t kMinFetchMs = 4000;

    telemetry::TelemetryStore& store_;
    MapExchange&               exchange_;
    IMapImageProvider&         provider_;

    uint8_t  zoom_        = 17;
    uint16_t edgeMargin_  = 150;
    uint32_t lastFetchMs_ = 0;
};

}  // namespace gmap

#endif /* MAP_CONTROLLER_H */
