#include "map/MapProjection.h"

namespace gmap {

MapProjection::MapProjection(const GeoBounds& bounds, uint16_t width, uint16_t height)
    : b_(bounds), w_(width), h_(height) {
    valid_ = (w_ > 0 && h_ > 0 &&
              b_.latTop != b_.latBottom && b_.lonRight != b_.lonLeft);
}

bool MapProjection::toPixel(double lat, double lon, int32_t& px, int32_t& py) const {
    if (!valid_) return false;
    const double lonRange = b_.lonRight - b_.lonLeft;
    const double latRange = b_.latTop - b_.latBottom;
    px = (int32_t)(((lon - b_.lonLeft) / lonRange) * w_);
    py = (int32_t)(((b_.latTop - lat) / latRange) * h_);
    return (px >= 0 && px < w_ && py >= 0 && py < h_);
}

bool MapProjection::toGeo(int32_t px, int32_t py, double& lat, double& lon) const {
    if (!valid_) return false;
    lon = b_.lonLeft + ((double)px / w_) * (b_.lonRight - b_.lonLeft);
    lat = b_.latTop  - ((double)py / h_) * (b_.latTop - b_.latBottom);
    return true;
}

}  // namespace gmap
