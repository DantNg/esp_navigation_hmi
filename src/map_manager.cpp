/**
 * @file map_manager.cpp
 * @brief Tile-based map viewer with manual pan for ESP32-S3
 *
 * Architecture:
 *   - Map is pre-split into 256x256 RGB565 tiles on SD card stored in PSRAM
 *   - A plain (NON-scrollable) LVGL panel acts as the viewport
 *   - Tile images are children positioned at SCREEN coords: col*TILE_SIZE - view_x
 *   - Touch LV_EVENT_PRESSING calculates delta and moves tiles instantly (pure math)
 *   - SD card reads happen ONLY in a 50ms timer - NEVER in touch event handler
 *
 * Why manual pan instead of LVGL native scroll?
 *   - LVGL native scroll recalculates layout/bounds when children are repositioned,
 *     which resets the scroll position mid-drag causing the "snap back" bug.
 *   - Manual pan gives full control: tile positions update via subtraction only.
 */

#include "map_manager.h"

#include <SD.h>
#include <SPI.h>
#include <FS.h>

/* ============================================================
 * Internal Data Structures
 * ============================================================ */

/** Represents one cached tile in PSRAM */
typedef struct {
    int16_t col;                /* Tile column index (-1 = empty) */
    int16_t row;                /* Tile row index */
    uint8_t *data;              /* Pixel data buffer in PSRAM */
    lv_img_dsc_t img_dsc;       /* LVGL image descriptor */
    lv_obj_t *img_obj;          /* LVGL image widget */
    bool loaded;                /* Whether valid data is loaded */
    uint32_t last_used;         /* Frame counter for LRU eviction */
    /* Cache last rendered state to skip redundant LVGL API calls */
    lv_coord_t last_sx;         /* Last screen X passed to lv_obj_set_pos */
    lv_coord_t last_sy;         /* Last screen Y passed to lv_obj_set_pos */
    bool last_hidden;           /* Last known visibility state */
} tile_slot_t;

/** Full module state */
static struct {
    /* Map dimensions */
    uint16_t map_w;
    uint16_t map_h;
    uint16_t tile_cols;
    uint16_t tile_rows;

    /* Current viewport top-left corner in map pixel coordinates */
    int32_t view_x;
    int32_t view_y;

    /* Touch tracking for manual pan */
    int32_t touch_last_x;
    int32_t touch_last_y;
    bool    touching;

    /* GPS bounds */
    double gps_lat_top;
    double gps_lon_left;
    double gps_lat_bottom;
    double gps_lon_right;
    bool gps_bounds_set;

    /* Marker position in map pixel coordinates */
    int32_t marker_x;
    int32_t marker_y;

    /* LVGL objects */
    lv_obj_t *map_cont;         /* Plain panel - viewport (800x480), NOT scrollable */
    lv_obj_t *marker_obj;       /* Triangle marker widget */

    /* Tile cache */
    tile_slot_t tiles[TILE_CACHE_MAX];
    uint32_t frame_counter;     /* Incremented each refresh for LRU */

    /* Dirty flag: set when view moves, cleared after all visible tiles loaded */
    volatile bool tiles_dirty;

    /* Timer for deferred SD card reads */
    lv_timer_t *tile_load_timer;

    bool initialized;
} ms;

/* ============================================================
 * SD Card
 * ============================================================ */

static bool init_sd_card(void) {
    SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);

    /* Try 25MHz first, fallback to 10MHz for slower cards */
    if (!SD.begin(SD_PIN_CS, SPI, 25000000)) {
        if (!SD.begin(SD_PIN_CS, SPI, 10000000)) {
            Serial.println("[MAP] SD card init failed!");
            return false;
        }
        Serial.println("[MAP] SD running at 10MHz");
    } else {
        Serial.println("[MAP] SD running at 25MHz");
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[MAP] No SD card detected!");
        return false;
    }

    Serial.printf("[MAP] SD card ready, size: %lluMB\n",
                  SD.cardSize() / (1024 * 1024));
    return true;
}

static bool read_map_config(void) {
    File f = SD.open("/map/config.txt", FILE_READ);
    if (!f) {
        Serial.println("[MAP] Cannot open /map/config.txt");
        return false;
    }

    String line = f.readStringUntil('\n');
    f.close();

    int comma = line.indexOf(',');
    if (comma < 0) {
        Serial.println("[MAP] Invalid config.txt format");
        return false;
    }

    ms.map_w = (uint16_t)line.substring(0, comma).toInt();
    ms.map_h = (uint16_t)line.substring(comma + 1).toInt();

    if (ms.map_w == 0 || ms.map_h == 0) {
        Serial.println("[MAP] Invalid map dimensions in config.txt");
        return false;
    }

    ms.tile_cols = (ms.map_w + TILE_SIZE - 1) / TILE_SIZE;
    ms.tile_rows = (ms.map_h + TILE_SIZE - 1) / TILE_SIZE;

    Serial.printf("[MAP] Map: %dx%d px, Tiles: %dx%d grid (%d total)\n",
                  ms.map_w, ms.map_h,
                  ms.tile_cols, ms.tile_rows,
                  ms.tile_cols * ms.tile_rows);
    return true;
}

static bool load_tile_data(tile_slot_t *slot, int16_t col, int16_t row) {
    char path[32];
    snprintf(path, sizeof(path), "/map/tile_%03d_%03d.bin", row, col);

    File f = SD.open(path, FILE_READ);
    if (!f) {
        memset(slot->data, 0x42, TILE_DATA_SIZE);
        Serial.printf("[MAP] Missing tile: %s\n", path);
        slot->col = col;
        slot->row = row;
        slot->loaded = true;
        return false;
    }

    size_t bytes_read = f.read(slot->data, TILE_DATA_SIZE);
    f.close();

    if (bytes_read != (size_t)TILE_DATA_SIZE) {
        Serial.printf("[MAP] Tile incomplete: %s (%d/%d)\n",
                      path, bytes_read, TILE_DATA_SIZE);
    }

    slot->col = col;
    slot->row = row;
    slot->loaded = true;
    return true;
}

/* ============================================================
 * Viewport Clamp
 * ============================================================ */

static void clamp_view(void) {
    int32_t max_x = (int32_t)ms.map_w - VIEWPORT_W;
    int32_t max_y = (int32_t)ms.map_h - VIEWPORT_H;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (ms.view_x < 0)     ms.view_x = 0;
    if (ms.view_y < 0)     ms.view_y = 0;
    if (ms.view_x > max_x) ms.view_x = max_x;
    if (ms.view_y > max_y) ms.view_y = max_y;
}

/* ============================================================
 * Tile Cache with PSRAM
 * ============================================================ */

static bool allocate_tile_buffers(void) {
    for (int i = 0; i < TILE_CACHE_MAX; i++) {
        tile_slot_t *t = &ms.tiles[i];

        t->data = (uint8_t *)ps_malloc(TILE_DATA_SIZE);
        if (t->data == NULL) {
            Serial.printf("[MAP] PSRAM alloc failed for slot %d\n", i);
            return false;
        }
        memset(t->data, 0x00, TILE_DATA_SIZE);

        t->img_dsc.header.always_zero = 0;
        t->img_dsc.header.w  = TILE_SIZE;
        t->img_dsc.header.h  = TILE_SIZE;
        t->img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        t->img_dsc.data_size = TILE_DATA_SIZE;
        t->img_dsc.data      = t->data;

        /* Child of the viewport panel – positioned in SCREEN coords */
        t->img_obj = lv_img_create(ms.map_cont);
        lv_img_set_src(t->img_obj, &t->img_dsc);
        lv_obj_add_flag(t->img_obj, LV_OBJ_FLAG_HIDDEN);
        /* Tiles must NOT consume touch – pass through to the container */
        lv_obj_clear_flag(t->img_obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(t->img_obj, LV_OBJ_FLAG_SCROLLABLE);

        t->col = -1;
        t->row = -1;
        t->loaded = false;
        t->last_used = 0;
        t->last_sx = -9999;
        t->last_sy = -9999;
        t->last_hidden = true;   /* starts hidden, matches LV_OBJ_FLAG_HIDDEN above */
    }

    Serial.printf("[MAP] Allocated %d tile buffers in PSRAM (%.1f KB each, %.1f KB total)\n",
                  TILE_CACHE_MAX,
                  TILE_DATA_SIZE / 1024.0f,
                  (TILE_CACHE_MAX * TILE_DATA_SIZE) / 1024.0f);
    return true;
}

/* ============================================================
 * Tile & Marker Positioning (pure math – no SD I/O)
 * Translates map coords → screen coords given current view_x / view_y.
 * Called every touch delta AND after loading new tiles from SD.
 * ============================================================ */

static void reposition_all_tiles(void) {
    for (int i = 0; i < TILE_CACHE_MAX; i++) {
        tile_slot_t *t = &ms.tiles[i];
        if (t->col < 0 || !t->loaded) continue;

        lv_coord_t sx = (lv_coord_t)(t->col * TILE_SIZE - ms.view_x);
        lv_coord_t sy = (lv_coord_t)(t->row * TILE_SIZE - ms.view_y);

        bool visible = (sx < VIEWPORT_W && sy < VIEWPORT_H &&
                        sx + TILE_SIZE > 0 && sy + TILE_SIZE > 0);

        if (visible) {
            /* Only call set_pos when coords actually changed */
            if (sx != t->last_sx || sy != t->last_sy) {
                lv_obj_set_pos(t->img_obj, sx, sy);
                t->last_sx = sx;
                t->last_sy = sy;
            }
            /* Only clear hidden flag when changing hidden→visible */
            if (t->last_hidden) {
                lv_obj_clear_flag(t->img_obj, LV_OBJ_FLAG_HIDDEN);
                t->last_hidden = false;
            }
        } else {
            /* Only add hidden flag when changing visible→hidden */
            if (!t->last_hidden) {
                lv_obj_add_flag(t->img_obj, LV_OBJ_FLAG_HIDDEN);
                t->last_hidden = true;
            }
        }
    }

    /* Update marker. No lv_obj_move_foreground here – marker is created after
     * all tile img_obj widgets so it is naturally on top in LVGL z-order.
     * Calling move_foreground every frame is expensive (marks whole parent dirty). */
    if (ms.marker_obj) {
        lv_coord_t mx = (lv_coord_t)(ms.marker_x - MARKER_SIZE / 2 - ms.view_x);
        lv_coord_t my = (lv_coord_t)(ms.marker_y - MARKER_SIZE / 2 - ms.view_y);
        lv_obj_set_pos(ms.marker_obj, mx, my);
    }
}

/* ============================================================
 * Touch Pan Handler – ONLY math, NO SD I/O
 * ============================================================ */

static void pan_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        ms.touch_last_x = pt.x;
        ms.touch_last_y = pt.y;
        ms.touching = true;
    }
    else if (code == LV_EVENT_PRESSING) {
        if (!ms.touching) return;
        lv_indev_t *indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);

        int32_t dx = ms.touch_last_x - pt.x;  /* drag left → map moves right */
        int32_t dy = ms.touch_last_y - pt.y;
        if (dx == 0 && dy == 0) return;

        ms.touch_last_x = pt.x;
        ms.touch_last_y = pt.y;
        ms.view_x += dx;
        ms.view_y += dy;
        clamp_view();

        reposition_all_tiles();   /* instant – pure subtraction */
        ms.tiles_dirty = true;    /* let timer load new tiles */
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ms.touching   = false;
        ms.tiles_dirty = true;
    }
}

/* ============================================================
 * Triangle Marker
 * ============================================================ */

static void marker_draw_cb(lv_event_t *e) {
    lv_obj_t *obj           = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(MARKER_COLOR);
    dsc.bg_opa   = LV_OPA_COVER;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_point_t pts[3] = {
        { (lv_coord_t)((coords.x1 + coords.x2) / 2), coords.y1 },
        { coords.x1,                                  coords.y2 },
        { coords.x2,                                  coords.y2 }
    };
    lv_draw_polygon(draw_ctx, &dsc, pts, 3);
}

static void create_marker(void) {
    ms.marker_obj = lv_obj_create(ms.map_cont);
    lv_obj_set_size(ms.marker_obj, MARKER_SIZE, MARKER_SIZE);
    lv_obj_set_style_bg_opa(ms.marker_obj,     LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ms.marker_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(ms.marker_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ms.marker_obj, 0, 0);
    lv_obj_clear_flag(ms.marker_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ms.marker_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ms.marker_obj, marker_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    ms.marker_x = ms.map_w / 2;
    ms.marker_y = ms.map_h / 2;
}

/* ============================================================
 * Tile Load Logic (runs in 50ms timer – SD I/O happens here only)
 * ============================================================ */

static void refresh_visible_tiles(void) {
    if (!ms.map_cont) return;

    int first_col = ms.view_x / TILE_SIZE;
    int first_row = ms.view_y / TILE_SIZE;
    int last_col  = (ms.view_x + VIEWPORT_W - 1) / TILE_SIZE;
    int last_row  = (ms.view_y + VIEWPORT_H - 1) / TILE_SIZE;

    if (first_col < 0) first_col = 0;
    if (first_row < 0) first_row = 0;
    if (last_col >= ms.tile_cols) last_col = ms.tile_cols - 1;
    if (last_row >= ms.tile_rows) last_row = ms.tile_rows - 1;

    ms.frame_counter++;

    bool slot_needed[TILE_CACHE_MAX] = {};
    struct { int16_t col, row; int slot; } needed[TILE_CACHE_MAX];
    int need_cnt = 0;

    for (int r = first_row; r <= last_row && need_cnt < TILE_CACHE_MAX; r++) {
        for (int c = first_col; c <= last_col && need_cnt < TILE_CACHE_MAX; c++) {
            needed[need_cnt].col  = (int16_t)c;
            needed[need_cnt].row  = (int16_t)r;
            needed[need_cnt].slot = -1;

            for (int i = 0; i < TILE_CACHE_MAX; i++) {
                if (ms.tiles[i].col == c && ms.tiles[i].row == r && ms.tiles[i].loaded) {
                    needed[need_cnt].slot = i;
                    slot_needed[i] = true;
                    ms.tiles[i].last_used = ms.frame_counter;
                    break;
                }
            }
            need_cnt++;
        }
    }

    /* Load up to 3 tiles per tick – fills a fresh viewport in 2-3 timer ticks
     * instead of 9+ ticks. Each SD read is ~128KB; 3 reads ≈ 60-90ms at 10MHz. */
    int  loaded_this_tick = 0;
    const int MAX_LOADS_PER_TICK = 3;
    bool more_needed = false;

    for (int n = 0; n < need_cnt; n++) {
        if (needed[n].slot >= 0) continue;      /* Cache hit – skip */
        if (loaded_this_tick >= MAX_LOADS_PER_TICK) { more_needed = true; break; }

        /* LRU eviction */
        int best = -1;
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < TILE_CACHE_MAX; i++) {
            if (!slot_needed[i] && ms.tiles[i].last_used < oldest) {
                oldest = ms.tiles[i].last_used;
                best = i;
            }
        }
        if (best < 0) break;

        needed[n].slot    = best;
        slot_needed[best] = true;

        load_tile_data(&ms.tiles[best], needed[n].col, needed[n].row);
        ms.tiles[best].last_used   = ms.frame_counter;
        ms.tiles[best].last_hidden = true;  /* reposition_all_tiles will show it */

        lv_img_set_src(ms.tiles[best].img_obj, &ms.tiles[best].img_dsc);
        loaded_this_tick++;
    }

    /* Reposition all tiles (and marker) to current screen coordinates */
    reposition_all_tiles();

    ms.tiles_dirty = more_needed;
}

static void tile_load_timer_cb(lv_timer_t *timer) {
    if (ms.tiles_dirty) {
        refresh_visible_tiles();
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

bool map_init(lv_obj_t *parent_screen) {
    memset(&ms, 0, sizeof(ms));
    Serial.println("[MAP] Initializing tile map (manual pan)...");

    /* 1. SD card */
    if (!init_sd_card()) {
        Serial.println("[MAP] FAILED: SD card");
        return false;
    }

    /* 2. Map dimensions from config.txt */
    if (!read_map_config()) {
        Serial.println("[MAP] FAILED: config");
        return false;
    }

    /* 3. Create a plain (NON-scrollable) viewport panel.
     *    We handle all panning ourselves so LVGL never fights us. */
    ms.map_cont = lv_obj_create(parent_screen);
    lv_obj_set_size(ms.map_cont, VIEWPORT_W, VIEWPORT_H);
    lv_obj_set_pos(ms.map_cont, 0, 0);
    lv_obj_set_style_bg_opa(ms.map_cont,     LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(ms.map_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ms.map_cont, 0, 0);

    /* Disable ALL scrolling */
    lv_obj_clear_flag(ms.map_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ms.map_cont, LV_DIR_NONE);

    /* Must be CLICKABLE to receive touch events */
    lv_obj_add_flag(ms.map_cont, LV_OBJ_FLAG_CLICKABLE);

    /* Register manual pan handlers */
    lv_obj_add_event_cb(ms.map_cont, pan_event_cb, LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(ms.map_cont, pan_event_cb, LV_EVENT_PRESSING,   NULL);
    lv_obj_add_event_cb(ms.map_cont, pan_event_cb, LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(ms.map_cont, pan_event_cb, LV_EVENT_PRESS_LOST, NULL);

    /* 4. Allocate PSRAM tile buffers */
    if (!allocate_tile_buffers()) {
        Serial.println("[MAP] FAILED: PSRAM alloc");
        return false;
    }

    /* 5. Triangle marker */
    create_marker();

    /* 6. Start viewport at center of map */
    ms.view_x = ((int32_t)ms.map_w - VIEWPORT_W) / 2;
    ms.view_y = ((int32_t)ms.map_h - VIEWPORT_H) / 2;
    clamp_view();

    /* 7. Deferred tile load timer (50 ms) */
    ms.tiles_dirty     = true;
    ms.tile_load_timer = lv_timer_create(tile_load_timer_cb, 20, NULL);

    /* Initial load for the starting view */
    refresh_visible_tiles();

    ms.initialized = true;
    Serial.println("[MAP] Ready – touch and drag to pan.");
    return true;
}

void map_set_center(int32_t center_x, int32_t center_y) {
    if (!ms.initialized) return;
    ms.view_x = center_x - VIEWPORT_W / 2;
    ms.view_y = center_y - VIEWPORT_H / 2;
    clamp_view();
    ms.tiles_dirty = true;
    reposition_all_tiles();
}

void map_pan(int32_t dx, int32_t dy) {
    if (!ms.initialized) return;
    ms.view_x += dx;
    ms.view_y += dy;
    clamp_view();
    ms.tiles_dirty = true;
    reposition_all_tiles();
}

void map_get_center(int32_t *cx, int32_t *cy) {
    if (!ms.initialized) return;
    if (cx) *cx = ms.view_x + VIEWPORT_W / 2;
    if (cy) *cy = ms.view_y + VIEWPORT_H / 2;
}

void map_get_size(uint16_t *width, uint16_t *height) {
    if (width)  *width  = ms.map_w;
    if (height) *height = ms.map_h;
}

void map_set_gps_bounds(double lat_top, double lon_left,
                        double lat_bottom, double lon_right) {
    ms.gps_lat_top    = lat_top;
    ms.gps_lon_left   = lon_left;
    ms.gps_lat_bottom = lat_bottom;
    ms.gps_lon_right  = lon_right;
    ms.gps_bounds_set = true;
    Serial.printf("[MAP] GPS bounds: lat[%.6f..%.6f] lon[%.6f..%.6f]\n",
                  lat_bottom, lat_top, lon_left, lon_right);
}

bool map_gps_to_pixel(double lat, double lon, int32_t *px, int32_t *py) {
    if (!ms.gps_bounds_set) return false;
    double lat_range = ms.gps_lat_top - ms.gps_lat_bottom;
    double lon_range = ms.gps_lon_right - ms.gps_lon_left;
    if (lat_range == 0.0 || lon_range == 0.0) return false;
    if (lat < ms.gps_lat_bottom || lat > ms.gps_lat_top)   return false;
    if (lon < ms.gps_lon_left   || lon > ms.gps_lon_right) return false;
    if (px) *px = (int32_t)(((lon - ms.gps_lon_left) / lon_range) * ms.map_w);
    if (py) *py = (int32_t)(((ms.gps_lat_top - lat)  / lat_range) * ms.map_h);
    return true;
}

bool map_pixel_to_gps(int32_t px, int32_t py, double *lat, double *lon) {
    if (!ms.gps_bounds_set || ms.map_w == 0 || ms.map_h == 0) return false;
    if (lon) *lon = ms.gps_lon_left + ((double)px / ms.map_w) *
                   (ms.gps_lon_right - ms.gps_lon_left);
    if (lat) *lat = ms.gps_lat_top  - ((double)py / ms.map_h) *
                   (ms.gps_lat_top  - ms.gps_lat_bottom);
    return true;
}

void map_set_marker(int32_t map_x, int32_t map_y) {
    ms.marker_x = map_x;
    ms.marker_y = map_y;
    reposition_all_tiles();   /* also moves marker to new screen position */
}

void map_get_marker(int32_t *map_x, int32_t *map_y) {
    if (map_x) *map_x = ms.marker_x;
    if (map_y) *map_y = ms.marker_y;
}

lv_obj_t *map_get_marker_obj(void) {
    return ms.marker_obj;
}

bool map_is_initialized(void) {
    return ms.initialized;
}

void map_refresh_tiles(void) {
    if (!ms.initialized) return;
    refresh_visible_tiles();
}
