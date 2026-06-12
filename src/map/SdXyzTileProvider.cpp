#include "map/SdXyzTileProvider.h"

#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdio>

#include "map/SdCardMount.h"

namespace gmap {

/* ── destructor ─────────────────────────────────────────────────────────── */

SdXyzTileProvider::~SdXyzTileProvider() {
    if (tileBuf_) {
        heap_caps_free(tileBuf_);
        tileBuf_ = nullptr;
    }
}

/* ── public: begin() ─────────────────────────────────────────────────────── */

bool SdXyzTileProvider::begin() {
    if (mounted_) return true;

    /* Allocate a single-tile PSRAM buffer (256×256×2 = 131 072 bytes).       */
    /* We never put tile data on the stack — blitTile() reuses this buffer.   */
    if (!tileBuf_) {
        tileBuf_ = static_cast<uint8_t*>(
            heap_caps_malloc(kTilePx * kTilePx * 2, MALLOC_CAP_SPIRAM));
        if (!tileBuf_) {
            Serial.println("[XYZ] PSRAM alloc failed");
            return false;
        }
    }

    if (!sdMount()) return false;

    if (!SD.exists("/tiles")) {
        Serial.println("[XYZ] /tiles not found on SD card");
        Serial.println("[XYZ]   run: python tools/osm_to_tiles.py map.osm --zoom 17 --out sd_tiles");
        Serial.println("[XYZ]   then copy sd_tiles/ to SD card as /tiles/");
        return false;
    }

    mounted_ = true;
    Serial.println("[XYZ] ready — /tiles found");
    return true;
}

/* ── Web Mercator helpers ───────────────────────────────────────────────── */

void SdXyzTileProvider::latLonToGlobalPixel(
        double lat, double lon, int zoom,
        double& gpx, double& gpy) {
    const double n   = static_cast<double>(1 << zoom);
    const double lat_rad = lat * M_PI / 180.0;
    gpx = (lon + 180.0) / 360.0 * n * kTilePx;
    gpy = (1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / M_PI) / 2.0 * n * kTilePx;
}

void SdXyzTileProvider::globalPixelToLatLon(
        double gpx, double gpy, int zoom,
        double& lat, double& lon) {
    const double n = static_cast<double>(1 << zoom);
    lon = gpx / (n * kTilePx) * 360.0 - 180.0;
    const double y = gpy / (n * kTilePx);
    lat = atan(sinh(M_PI * (1.0 - 2.0 * y))) * 180.0 / M_PI;
}

/* ── tile file loading ──────────────────────────────────────────────────── */

bool SdXyzTileProvider::loadTile(int zoom, int tx, int ty) {
    if (loadedZoom_ == zoom && loadedTx_ == tx && loadedTy_ == ty) return true;

    char path[48];
    snprintf(path, sizeof(path), "/tiles/%d/%d/%d.bin", zoom, tx, ty);

    File f = SD.open(path, FILE_READ);
    if (!f) {
        /* Tile not downloaded — fill with a mid-grey so missing tiles are visible. */
        const uint16_t grey = 0x8410;   /* RGB565 ~mid grey */
        const int count = kTilePx * kTilePx;
        uint16_t* p = reinterpret_cast<uint16_t*>(tileBuf_);
        for (int i = 0; i < count; i++) p[i] = grey;
        return false;
    }

    const size_t expected = static_cast<size_t>(kTilePx * kTilePx * 2);
    size_t got = f.read(tileBuf_, expected);
    f.close();

    if (got != expected) {
        Serial.printf("[XYZ] short read %s: %u/%u\n", path, (unsigned)got, (unsigned)expected);
        return false;
    }

    loadedZoom_ = zoom; loadedTx_ = tx; loadedTy_ = ty;
    return true;
}

/* ── blit loaded tile into output viewport ──────────────────────────────── */

void SdXyzTileProvider::blitTile(int zoom, int tx, int ty,
                                  int32_t vpGpxLeft, int32_t vpGpyTop,
                                  const MapImage& out) {
    loadTile(zoom, tx, ty);   /* fills tileBuf_ (grey on miss) */

    /* Tile extents in global-pixel space */
    const int32_t tpxL = tx * kTilePx;
    const int32_t tpxT = ty * kTilePx;
    const int32_t tpxR = tpxL + kTilePx;
    const int32_t tpxB = tpxT + kTilePx;

    /* Viewport extents */
    const int32_t vpR = vpGpxLeft + out.w;
    const int32_t vpB = vpGpyTop  + out.h;

    /* Intersection */
    const int32_t srcX0 = std::max(tpxL, vpGpxLeft) - tpxL;   /* within tile */
    const int32_t srcY0 = std::max(tpxT, vpGpyTop)  - tpxT;
    const int32_t srcX1 = std::min(tpxR, vpR)        - tpxL;
    const int32_t srcY1 = std::min(tpxB, vpB)        - tpxT;

    if (srcX0 >= srcX1 || srcY0 >= srcY1) return;   /* no overlap */

    const int32_t dstX0 = tpxL + srcX0 - vpGpxLeft;
    const int32_t dstY0 = tpxT + srcY0 - vpGpyTop;
    const int32_t copyW = srcX1 - srcX0;

    const uint16_t* src = reinterpret_cast<const uint16_t*>(tileBuf_);
    uint16_t*       dst = reinterpret_cast<uint16_t*>(out.data);

    for (int32_t row = 0; row < (srcY1 - srcY0); row++) {
        const uint16_t* s = src + (srcY0 + row) * kTilePx + srcX0;
        uint16_t*       d = dst + (dstY0 + row) * out.w   + dstX0;
        memcpy(d, s, static_cast<size_t>(copyW) * 2);
    }
}

/* ── public: fetch() ─────────────────────────────────────────────────────── */

bool SdXyzTileProvider::fetch(double lat, double lon, uint8_t zoom, MapImage& out) {
    if (!mounted_ || !out.data) return false;

    /* Centre of the viewport in global-pixel space */
    double cGpx, cGpy;
    latLonToGlobalPixel(lat, lon, zoom, cGpx, cGpy);

    /* Top-left corner of viewport (may be negative near the poles/anti-meridian) */
    const int32_t vpL = static_cast<int32_t>(cGpx) - out.w / 2;
    const int32_t vpT = static_cast<int32_t>(cGpy) - out.h / 2;

    /* Which tiles span that viewport? */
    const int nTiles  = 1 << zoom;
    const int txFirst = static_cast<int>(floor(static_cast<double>(vpL) / kTilePx));
    const int tyFirst = static_cast<int>(floor(static_cast<double>(vpT) / kTilePx));
    const int txLast  = static_cast<int>(floor(static_cast<double>(vpL + out.w - 1) / kTilePx));
    const int tyLast  = static_cast<int>(floor(static_cast<double>(vpT + out.h - 1) / kTilePx));

    Serial.printf("[XYZ] fetch z=%u tiles [%d-%d, %d-%d] vp(%d,%d)\n",
                  zoom, txFirst, txLast, tyFirst, tyLast, vpL, vpT);

    /* Clear output buffer to dark-grey (shows unmapped areas clearly) */
    memset(out.data, 0x21, static_cast<size_t>(out.w) * out.h * 2);

    bool anyLoaded = false;
    for (int ty = tyFirst; ty <= tyLast; ty++) {
        for (int tx = txFirst; tx <= txLast; tx++) {
            /* Clamp to valid tile range (world wrap at x) */
            const int txx = ((tx % nTiles) + nTiles) % nTiles;
            const int tyy = ty;
            if (tyy < 0 || tyy >= nTiles) continue;

            blitTile(zoom, txx, tyy, vpL, vpT, out);
            anyLoaded = true;

            /* Yield so the core-0 idle task can feed the watchdog — a full
             * 1024×1024 viewport reads up to 25 tiles (~3 MB) from SD. */
            vTaskDelay(1);
        }
    }

    if (!anyLoaded) return false;

    /* Fill out.bounds from viewport corners */
    double latBL, lonBL, latTR, lonTR;
    globalPixelToLatLon(vpL,         vpT + out.h, zoom, latBL, lonBL);
    globalPixelToLatLon(vpL + out.w, vpT,         zoom, latTR, lonTR);
    out.bounds = GeoBounds{latTR, lonBL, latBL, lonTR};

    /* Cache the centre as default position */
    defaultLat_ = lat; defaultLon_ = lon; hasDefault_ = true;

    Serial.println("[XYZ] fetch done");
    return true;
}

/* ── public: getDefaultPosition() ───────────────────────────────────────── */

bool SdXyzTileProvider::getDefaultPosition(double& lat, double& lon) {
    if (!hasDefault_) return false;
    lat = defaultLat_;
    lon = defaultLon_;
    return true;
}

}  // namespace gmap
