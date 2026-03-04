/**
 * @file map_manager.h
 * @brief Tile-based map viewer for ESP32 with LVGL
 *
 * This module provides a scrollable map viewer that loads a large map
 * stored as tiles on an SD card. Only tiles visible in the 800x480
 * viewport are loaded into PSRAM at any time.
 *
 * Map preparation:
 *   Use tools/split_map.py to split a large image into tiles.
 *   Copy the output folder to SD card as /map/
 *
 * Tile format:
 *   - Raw RGB565, little-endian, 256x256 pixels per tile
 *   - Files: /map/tile_RRR_CCC.bin (row, column with 3 digits)
 *   - Config: /map/config.txt (first line: "width,height")
 */

#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * Tile System Configuration
 * ============================================================ */
#define TILE_SIZE           256     /* Each tile: 256x256 pixels */
#define TILE_PIXEL_BYTES    2       /* RGB565 = 2 bytes/pixel */
#define TILE_DATA_SIZE      (TILE_SIZE * TILE_SIZE * TILE_PIXEL_BYTES)

#define VIEWPORT_W          800     /* Display width */
#define VIEWPORT_H          480     /* Display height */

/* Max tiles in memory at once.
 * For 800x480 viewport with 256px tiles:
 *   Horizontal: ceil(800/256)+1 = 5
 *   Vertical:   ceil(480/256)+1 = 3
 *   Total: 5 x 3 = 15 tiles (~1.9MB PSRAM) */
#define TILE_CACHE_MAX      15

/* SD Card SPI pins for CrowPanel 5.0" ESP32-S3
 * Adjust these if your board uses different pins! */
#define SD_PIN_CS           10
#define SD_PIN_MOSI         11
#define SD_PIN_SCK          12
#define SD_PIN_MISO         13

/* Triangle marker configuration */
#define MARKER_SIZE         24      /* Marker size in pixels */
#define MARKER_COLOR        0xFF0000 /* Red */

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * Initialize the tile-based map system.
 * - Opens SD card and reads /map/config.txt
 * - Allocates PSRAM buffers for tile cache
 * - Creates LVGL objects (tile images, marker, container)
 *
 * @param parent_screen  LVGL screen to display the map on
 * @return true if initialization was successful
 */
bool map_init(lv_obj_t *parent_screen);

/**
 * Center the viewport on given map pixel coordinates.
 * Automatically loads/unloads tiles as needed.
 *
 * @param center_x  X coordinate in map pixels
 * @param center_y  Y coordinate in map pixels
 */
void map_set_center(int32_t center_x, int32_t center_y);

/**
 * Pan (scroll) the viewport by a pixel delta.
 * Positive dx = move viewport right, positive dy = move viewport down.
 */
void map_pan(int32_t dx, int32_t dy);

/** Get the current viewport center in map coordinates. */
void map_get_center(int32_t *cx, int32_t *cy);

/** Get total map dimensions in pixels. */
void map_get_size(uint16_t *width, uint16_t *height);

/**
 * Set GPS coordinate bounds for the map image.
 * These define the geographic area the map covers.
 *
 * @param lat_top     Latitude of the top edge (north)
 * @param lon_left    Longitude of the left edge (west)
 * @param lat_bottom  Latitude of the bottom edge (south)
 * @param lon_right   Longitude of the right edge (east)
 */
void map_set_gps_bounds(double lat_top, double lon_left,
                        double lat_bottom, double lon_right);

/**
 * Convert GPS latitude/longitude to map pixel coordinates.
 *
 * @return true if the GPS position is within map bounds
 */
bool map_gps_to_pixel(double lat, double lon, int32_t *px, int32_t *py);

/**
 * Convert map pixel coordinates back to GPS latitude/longitude.
 */
bool map_pixel_to_gps(int32_t px, int32_t py, double *lat, double *lon);

/**
 * Set the person marker position in MAP coordinates.
 * The marker is displayed at the correct screen position
 * based on the current viewport.
 */
void map_set_marker(int32_t map_x, int32_t map_y);

/** Get current marker position in map coordinates. */
void map_get_marker(int32_t *map_x, int32_t *map_y);

/** Get the LVGL marker object (for custom styling). */
lv_obj_t *map_get_marker_obj(void);

/** Check if the map system was initialized successfully. */
bool map_is_initialized(void);

/**
 * Force a tile refresh. Usually not needed since scroll events
 * trigger refreshes automatically. Can be called after map_set_center()
 * from code (not from touch).
 */
void map_refresh_tiles(void);

#ifdef __cplusplus
}
#endif

#endif /* MAP_MANAGER_H */
