/**
 * @file MapProjection.h
 * @brief Linear lat/lon <-> pixel mapping for one satellite map image.
 *
 * A map image covers a geographic rectangle (GeoBounds). Within the small area
 * a map tile spans, an equirectangular (linear) mapping is accurate enough and
 * matches how tools/map_server.py renders the image. Pure value type, copyable,
 * safe to pass between tasks.
 */
#ifndef MAP_PROJECTION_H
#define MAP_PROJECTION_H

#include <cstdint>

namespace gmap {

struct GeoBounds {
    double latTop    = 0;   /* north edge */
    double lonLeft   = 0;   /* west edge  */
    double latBottom = 0;   /* south edge */
    double lonRight  = 0;   /* east edge  */
};

class MapProjection {
public:
    MapProjection() = default;
    MapProjection(const GeoBounds& bounds, uint16_t width, uint16_t height);

    bool     valid()  const { return valid_; }
    uint16_t width()  const { return w_; }
    uint16_t height() const { return h_; }
    GeoBounds bounds() const { return b_; }

    /** Map a geographic point to image pixels. Returns true if inside [0,w)x[0,h). */
    bool toPixel(double lat, double lon, int32_t& px, int32_t& py) const;

    /** Map image pixels back to a geographic point. */
    bool toGeo(int32_t px, int32_t py, double& lat, double& lon) const;

private:
    GeoBounds b_{};
    uint16_t  w_ = 0;
    uint16_t  h_ = 0;
    bool      valid_ = false;
};

}  // namespace gmap

#endif /* MAP_PROJECTION_H */
