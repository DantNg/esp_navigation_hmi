#include "map/SdTileMapProvider.h"

#include <Arduino.h>
#include <SD.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "map/SdCardMount.h"

namespace gmap {

namespace {
int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Print first-level contents of a SD directory for diagnostics. */
void listDir(const char* path) {
    File dir = SD.open(path);
    if (!dir) {
        Serial.printf("[SDMAP]   (cannot open %s)\n", path);
        return;
    }
    Serial.printf("[SDMAP]   contents of %s:\n", path);
    int count = 0;
    while (true) {
        File f = dir.openNextFile();
        if (!f) break;
        Serial.printf("[SDMAP]     %s  (%lu bytes)\n",
                      f.name(), (unsigned long)f.size());
        f.close();
        if (++count >= 8) { Serial.println("[SDMAP]     ..."); break; }
    }
    dir.close();
}
}  // namespace

/* -------------------------------------------------------------------------- */
bool SdTileMapProvider::begin() {
    return mount() && loadConfig();
}

bool SdTileMapProvider::mount() {
    if (mounted_) return true;
    if (!sdMount()) return false;
    listDir("/");
    mounted_ = true;
    return true;
}

bool SdTileMapProvider::loadConfig() {
    if (configLoaded_) return true;

    /* Check the /map directory exists first */
    if (!SD.exists("/map")) {
        Serial.println("[SDMAP] /map folder not found on SD card");
        Serial.println("[SDMAP]   Expected structure:");
        Serial.println("[SDMAP]     /map/config.txt");
        Serial.println("[SDMAP]     /map/tile_000_000.bin  ...");
        listDir("/");
        return false;
    }

    File f = SD.open("/map/config.txt");
    if (!f) {
        Serial.println("[SDMAP] /map/config.txt not found");
        listDir("/map");
        return false;
    }

    /* Line 1: totalWidth,totalHeight */
    String line1 = f.readStringUntil('\n');
    line1.trim();
    int comma1 = line1.indexOf(',');
    if (comma1 < 0) {
        Serial.printf("[SDMAP] config.txt line1 parse error: \"%s\"\n",
                      line1.c_str());
        f.close();
        return false;
    }
    totalW_ = line1.substring(0, comma1).toInt();
    totalH_ = line1.substring(comma1 + 1).toInt();

    /* Line 2: latTop,lonLeft,latBottom,lonRight */
    String line2 = f.readStringUntil('\n');
    line2.trim();
    int c2 = line2.indexOf(',');
    int c3 = line2.indexOf(',', c2 + 1);
    int c4 = line2.indexOf(',', c3 + 1);
    if (c2 < 0 || c3 < 0 || c4 < 0) {
        Serial.printf("[SDMAP] config.txt line2 parse error: \"%s\"\n",
                      line2.c_str());
        f.close();
        return false;
    }
    latTop_    = line2.substring(0,      c2).toDouble();
    lonLeft_   = line2.substring(c2 + 1, c3).toDouble();
    latBottom_ = line2.substring(c3 + 1, c4).toDouble();
    lonRight_  = line2.substring(c4 + 1).toDouble();

    /* Line 3: tileSize */
    String line3 = f.readStringUntil('\n');
    line3.trim();
    if (line3.length() > 0) tileSize_ = line3.toInt();
    f.close();

    if (totalW_ <= 0 || totalH_ <= 0 || tileSize_ <= 0 ||
        tileSize_ > kMaxTileSize) {
        Serial.printf("[SDMAP] invalid config: w=%d h=%d tile=%d\n",
                      totalW_, totalH_, tileSize_);
        return false;
    }

    Serial.printf("[SDMAP] config OK — map %dx%d  tile %dpx\n",
                  totalW_, totalH_, tileSize_);
    Serial.printf("[SDMAP]   bounds: latTop=%.6f lonLeft=%.6f "
                  "latBot=%.6f lonRight=%.6f\n",
                  latTop_, lonLeft_, latBottom_, lonRight_);
    configLoaded_ = true;
    return true;
}

/* -------------------------------------------------------------------------- */
bool SdTileMapProvider::fetch(double lat, double lon, uint8_t /*zoom*/,
                               MapImage& out) {
    if (!mounted_ && !mount())           return false;
    if (!configLoaded_ && !loadConfig()) return false;
    if (!out.data || out.w == 0 || out.h == 0) return false;

    const double pxPerDegLon = (double)totalW_ / (lonRight_ - lonLeft_);
    const double pxPerDegLat = (double)totalH_ / (latTop_   - latBottom_);

    const double cx = (lon - lonLeft_)  * pxPerDegLon;
    const double cy = (latTop_ - lat)   * pxPerDegLat;

    const int32_t vpLeft   = (int32_t)(cx - out.w * 0.5);
    const int32_t vpTop    = (int32_t)(cy - out.h * 0.5);
    const int32_t vpRight  = vpLeft + (int32_t)out.w;
    const int32_t vpBottom = vpTop  + (int32_t)out.h;

    /* Check drone is within map bounds */
    if (cx < 0 || cx > totalW_ || cy < 0 || cy > totalH_) {
        Serial.printf("[SDMAP] drone pos (%.6f, %.6f) outside map bounds!\n",
                      lat, lon);
        return false;
    }

    memset(out.data, 0, (size_t)out.w * out.h * 2);

    const int32_t colStart = clamp(vpLeft   / tileSize_,         0, (totalW_ - 1) / tileSize_);
    const int32_t colEnd   = clamp((vpRight  - 1) / tileSize_,   0, (totalW_ - 1) / tileSize_);
    const int32_t rowStart = clamp(vpTop    / tileSize_,         0, (totalH_ - 1) / tileSize_);
    const int32_t rowEnd   = clamp((vpBottom - 1) / tileSize_,   0, (totalH_ - 1) / tileSize_);

    Serial.printf("[SDMAP] fetch lat=%.6f lon=%.6f  tiles [%d-%d][%d-%d]\n",
                  lat, lon, rowStart, rowEnd, colStart, colEnd);

    for (int32_t tr = rowStart; tr <= rowEnd; tr++) {
        for (int32_t tc = colStart; tc <= colEnd; tc++) {
            blitTile(tr, tc, vpLeft, vpTop, out);
            vTaskDelay(1);   /* let the idle task feed the watchdog */
        }
    }

    out.bounds.latTop    = latTop_ - (double)vpTop    / pxPerDegLat;
    out.bounds.latBottom = latTop_ - (double)vpBottom / pxPerDegLat;
    out.bounds.lonLeft   = lonLeft_ + (double)vpLeft  / pxPerDegLon;
    out.bounds.lonRight  = lonLeft_ + (double)vpRight / pxPerDegLon;
    return true;
}

/* -------------------------------------------------------------------------- */
void SdTileMapProvider::blitTile(int32_t tileRow, int32_t tileCol,
                                  int32_t vpLeft, int32_t vpTop,
                                  const MapImage& out) {
    char path[64];
    snprintf(path, sizeof(path), "/map/tile_%03d_%03d.bin",
             (int)tileRow, (int)tileCol);

    File f = SD.open(path);
    if (!f) {
        Serial.printf("[SDMAP]   tile missing: %s\n", path);
        return;
    }

    const int32_t srcLeft = tileCol * tileSize_;
    const int32_t srcTop  = tileRow * tileSize_;

    const int32_t overlapLeft   = std::max(srcLeft, vpLeft);
    const int32_t overlapTop    = std::max(srcTop,  vpTop);
    const int32_t overlapRight  = std::min(srcLeft + tileSize_, vpLeft + (int32_t)out.w);
    const int32_t overlapBottom = std::min(srcTop  + tileSize_, vpTop  + (int32_t)out.h);

    if (overlapLeft >= overlapRight || overlapTop >= overlapBottom) {
        f.close();
        return;
    }

    const int32_t rowLen = overlapRight - overlapLeft;
    for (int32_t sy = overlapTop; sy < overlapBottom; sy++) {
        const int32_t tileLocalRow = sy          - srcTop;
        const int32_t tileLocalCol = overlapLeft - srcLeft;
        const uint32_t fileOff =
            ((uint32_t)tileLocalRow * (uint32_t)tileSize_ + (uint32_t)tileLocalCol) * 2u;
        f.seek(fileOff);
        f.read(rowBuf_, (size_t)rowLen * 2);

        const int32_t dstX = overlapLeft - vpLeft;
        const int32_t dstY = sy          - vpTop;
        uint8_t* dst = out.data + ((size_t)dstY * out.w + (size_t)dstX) * 2;
        memcpy(dst, rowBuf_, (size_t)rowLen * 2);
    }
    f.close();
}

}  // namespace gmap
