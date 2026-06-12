#ifndef MAP_SD_CARD_MOUNT_H
#define MAP_SD_CARD_MOUNT_H

namespace gmap {

/**
 * Singleton SD mount helper.
 * Both SdTileMapProvider and SdXyzTileProvider call sdMount() instead of
 * constructing their own SPIClass — avoids two SPIClass(HSPI) objects
 * fighting over the same SPI2 hardware and causing a panic.
 */
bool sdMount();   /* idempotent: safe to call multiple times */
void sdUnmount();
bool sdMounted();

}  // namespace gmap

#endif
