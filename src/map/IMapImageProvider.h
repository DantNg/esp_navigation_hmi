/**
 * @file IMapImageProvider.h
 * @brief Abstraction over "give me a satellite image centered here".
 *
 * MapController depends on this, not on HTTP, so an SD-card or cache-backed
 * provider can be dropped in later without touching the controller (DIP/OCP).
 */
#ifndef I_MAP_IMAGE_PROVIDER_H
#define I_MAP_IMAGE_PROVIDER_H

#include <cstdint>

#include "map/MapProjection.h"

namespace gmap {

/** Destination + result descriptor for a fetch. `data` is caller-allocated. */
struct MapImage {
    uint8_t*  data = nullptr;  /* preallocated RGB565 buffer, w*h*2 bytes */
    uint16_t  w    = 0;
    uint16_t  h    = 0;
    GeoBounds bounds{};        /* filled by the provider on success */
};

class IMapImageProvider {
public:
    virtual ~IMapImageProvider() = default;

    /**
     * Fetch a satellite image centered on (lat, lon) at the given zoom into
     * out.data (must be w*h*2 bytes). On success fills out.bounds.
     */
    virtual bool fetch(double lat, double lon, uint8_t zoom, MapImage& out) = 0;

    /**
     * Optional: return a sensible default centre (e.g. the middle of a
     * pre-loaded tile set) to show before any GPS fix is available.
     * Returns false if the provider has no meaningful default.
     */
    virtual bool getDefaultPosition(double& lat, double& lon) { return false; }
};

}  // namespace gmap

#endif /* I_MAP_IMAGE_PROVIDER_H */
