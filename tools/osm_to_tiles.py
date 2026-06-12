#!/usr/bin/env python3
"""
osm_to_tiles.py  —  Render a .osm file to XYZ RGB565 tiles for the SD card.

No internet required. All rendering is done locally from the .osm file.

Output layout (copy to SD card as /tiles):
    <output_dir>/<zoom>/<x>/<y>.bin   — 256×256 px, RGB565 little-endian

Usage
-----
    python osm_to_tiles.py map.osm --zoom 15 16 17 --out ./sd_tiles

    # Specify a sub-area (faster for large .osm files)
    python osm_to_tiles.py city.osm --zoom 17 --bounds 21.082 105.780 21.068 105.796 --out ./sd_tiles

Requirements
------------
    pip install Pillow
    (no other dependencies — uses stdlib xml.etree)

How to get a .osm file
----------------------
    1. Go to https://www.openstreetmap.org
    2. Click "Export" → "Manually select a different area"
    3. Draw your area → click "Export" (downloads .osm XML)
    OR use Overpass API:
       https://overpass-api.de/api/map?bbox=105.78,21.07,105.80,21.09
    OR use JOSM / osmium for large areas.
"""

import argparse
import math
import os
import struct
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Missing Pillow. Install with:  pip install Pillow")
    sys.exit(1)

TILE_PX = 256

# ── colour palette ────────────────────────────────────────────────────────────
# (R, G, B) tuples

C_BACKGROUND   = (242, 239, 233)   # OSM-like beige land
C_WATER        = ( 97, 171, 220)   # blue water
C_PARK         = (200, 233, 180)   # green park/grass
C_FOREST       = (157, 202, 138)   # dark green woods
C_RESIDENTIAL  = (235, 232, 225)   # light grey residential area
C_COMMERCIAL   = (240, 230, 210)   # commercial area
C_INDUSTRIAL   = (223, 213, 203)   # industrial area
C_BUILDING     = (188, 185, 181)   # building fill
C_BUILDING_OUT = (155, 153, 150)   # building outline

# Roads — drawn in order: fill colour, then an optional outline colour
ROAD_STYLES = {
    # (fill_rgb, width_px_at_z17, outline_rgb_or_None)
    "motorway"      : ((232, 146,  10), 6, (204, 128,   9)),
    "trunk"         : ((250, 178,  11), 5, (220, 157,  10)),
    "primary"       : ((252, 214,  95), 5, (210, 178,  80)),
    "secondary"     : ((253, 242, 144), 4, (200, 192, 116)),
    "tertiary"      : (( 71, 213,  13), 3, None),
    "residential"   : ((255, 255, 255), 2, (180, 180, 180)),
    "service"       : ((255, 255, 255), 1, (170, 170, 170)),
    "unclassified"  : ((255, 255, 255), 2, (180, 180, 180)),
    "track"         : ((145, 112,  80), 1, None),
    "path"          : ((255, 182,  96), 1, None),
    "footway"       : ((255, 182,  96), 1, None),
    "cycleway"      : (( 78, 146, 227), 1, None),
    "steps"         : ((255, 182,  96), 1, None),
    "living_street" : ((255, 255, 255), 2, (180, 180, 180)),
    "pedestrian"    : ((246, 244, 238), 3, (204, 201, 195)),
}

ROAD_ORDER = [
    "track", "path", "footway", "cycleway", "steps",
    "living_street", "pedestrian", "service",
    "unclassified", "residential",
    "tertiary", "secondary", "primary", "trunk", "motorway",
]

# ── Web Mercator math ─────────────────────────────────────────────────────────

def lat_lon_to_global_px(lat, lon, zoom):
    n = 2 ** zoom * TILE_PX
    gx = (lon + 180) / 360 * n
    lat_r = math.radians(lat)
    gy = (1 - math.asinh(math.tan(lat_r)) / math.pi) / 2 * n
    return gx, gy

def lat_lon_to_tile(lat, lon, zoom):
    gx, gy = lat_lon_to_global_px(lat, lon, zoom)
    return int(gx / TILE_PX), int(gy / TILE_PX)

def tile_bounds(tx, ty, zoom):
    """Return (lat_top, lon_left, lat_bot, lon_right) for a tile."""
    n = 2 ** zoom
    lon_left  =  tx      / n * 360 - 180
    lon_right = (tx + 1) / n * 360 - 180
    lat_top   = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 *  ty      / n))))
    lat_bot   = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * (ty + 1) / n))))
    return lat_top, lon_left, lat_bot, lon_right

# ── RGB565 conversion ─────────────────────────────────────────────────────────

def image_to_rgb565(img: Image.Image) -> bytes:
    img = img.convert("RGB")
    pixels = img.load()
    w, h = img.size
    buf = bytearray(w * h * 2)
    idx = 0
    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            struct.pack_into("<H", buf, idx, v)
            idx += 2
    return bytes(buf)

# ── OSM parser ────────────────────────────────────────────────────────────────

class OsmData:
    def __init__(self):
        self.nodes  = {}   # node_id -> (lat, lon)
        self.ways   = []   # list of {tags: dict, node_ids: list}

    def load(self, path: str):
        print(f"Parsing {path} …")
        tree = ET.parse(path)
        root = tree.getroot()

        for elem in root.iter("node"):
            nid = int(elem.get("id"))
            lat = float(elem.get("lat"))
            lon = float(elem.get("lon"))
            self.nodes[nid] = (lat, lon)

        for elem in root.iter("way"):
            tags = {t.get("k"): t.get("v") for t in elem.findall("tag")}
            nrefs = [int(nd.get("ref")) for nd in elem.findall("nd")]
            if nrefs:
                self.ways.append({"tags": tags, "node_ids": nrefs})

        print(f"  {len(self.nodes):,} nodes, {len(self.ways):,} ways")

    def bounds(self):
        if not self.nodes:
            return None
        lats = [v[0] for v in self.nodes.values()]
        lons = [v[1] for v in self.nodes.values()]
        return min(lats), min(lons), max(lats), max(lons)

# ── tile renderer ─────────────────────────────────────────────────────────────

def _scale(lat, lon, lat_top, lon_left, lat_bot, lon_right):
    """Map lat/lon to pixel (float) within a 256×256 tile."""
    x = (lon - lon_left)  / (lon_right  - lon_left)  * TILE_PX
    y = (lat_top - lat)   / (lat_top    - lat_bot)   * TILE_PX
    return x, y


def render_tile(osm: OsmData, tx: int, ty: int, zoom: int) -> Image.Image:
    lat_top, lon_left, lat_bot, lon_right = tile_bounds(tx, ty, zoom)

    # Add a small margin so features near the edge don't get clipped
    dlat = lat_top - lat_bot
    dlon = lon_right - lon_left
    M = 0.05   # 5% margin
    lat_top_m  = lat_top  + dlat * M
    lat_bot_m  = lat_bot  - dlat * M
    lon_left_m = lon_left - dlon * M
    lon_right_m= lon_right+ dlon * M

    img  = Image.new("RGB", (TILE_PX, TILE_PX), C_BACKGROUND)
    draw = ImageDraw.Draw(img)

    def coords(node_ids):
        pts = []
        for nid in node_ids:
            if nid not in osm.nodes:
                continue
            la, lo = osm.nodes[nid]
            if la < lat_bot_m or la > lat_top_m or lo < lon_left_m or lo > lon_right_m:
                # keep if at least one adjacent node is inside — skip lone outliers
                pass
            x, y = _scale(la, lo, lat_top, lon_left, lat_bot, lon_right)
            pts.append((x, y))
        return pts

    # ── Layer 0: area fills (water, parks, buildings) ──────────────────────
    for way in osm.ways:
        tags = way["tags"]
        nids = way["node_ids"]
        pts = coords(nids)
        if len(pts) < 3:
            continue

        natural  = tags.get("natural", "")
        landuse  = tags.get("landuse", "")
        waterway = tags.get("waterway", "")
        leisure  = tags.get("leisure", "")
        building = tags.get("building", "")
        amenity  = tags.get("amenity", "")

        if natural in ("water", "wetland", "bay") or waterway in ("riverbank", "dock"):
            draw.polygon(pts, fill=C_WATER)
        elif natural in ("wood", "tree_row", "scrub", "heath"):
            draw.polygon(pts, fill=C_FOREST)
        elif landuse in ("forest",):
            draw.polygon(pts, fill=C_FOREST)
        elif landuse in ("grass", "meadow", "farmland", "village_green") \
                or leisure in ("park", "garden", "pitch", "playground"):
            draw.polygon(pts, fill=C_PARK)
        elif landuse == "residential":
            draw.polygon(pts, fill=C_RESIDENTIAL)
        elif landuse in ("commercial", "retail"):
            draw.polygon(pts, fill=C_COMMERCIAL)
        elif landuse == "industrial":
            draw.polygon(pts, fill=C_INDUSTRIAL)
        elif building:
            draw.polygon(pts, fill=C_BUILDING, outline=C_BUILDING_OUT)

    # ── Layer 1: waterways (lines) ─────────────────────────────────────────
    for way in osm.ways:
        tags = way["tags"]
        ww = tags.get("waterway", "")
        if ww in ("river", "stream", "canal", "drain"):
            pts = coords(way["node_ids"])
            if len(pts) >= 2:
                w = 3 if ww in ("river", "canal") else 1
                draw.line(pts, fill=C_WATER, width=w)

    # ── Layer 2: roads (back-to-front by importance) ───────────────────────
    scale_factor = 2 ** (zoom - 17)   # thinner roads at low zoom

    for road_type in ROAD_ORDER:
        style = ROAD_STYLES.get(road_type)
        if not style:
            continue
        fill_col, base_w, outline_col = style
        w = max(1, int(base_w * scale_factor))

        for way in osm.ways:
            tags = way["tags"]
            hw = tags.get("highway", "")
            if hw != road_type:
                continue
            pts = coords(way["node_ids"])
            if len(pts) < 2:
                continue
            if outline_col and w >= 2:
                draw.line(pts, fill=outline_col, width=w + 2)
            draw.line(pts, fill=fill_col, width=w)

    return img


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Render a .osm file to XYZ RGB565 tiles for the SD card.")
    parser.add_argument("osm_file", help=".osm file to render")
    parser.add_argument("--zoom", "-z", nargs="+", type=int, default=[15, 17],
                        help="Zoom level(s) to render (default: 15 17)")
    parser.add_argument("--out", "-o", default="./sd_tiles",
                        help="Output directory (copy to SD card as /tiles)")
    parser.add_argument("--bounds", nargs=4, type=float,
                        metavar=("LAT_TOP", "LON_LEFT", "LAT_BOTTOM", "LON_RIGHT"),
                        help="Render only this sub-area (optional, defaults to full .osm extent)")
    args = parser.parse_args()

    osm = OsmData()
    osm.load(args.osm_file)

    if not osm.nodes:
        print("ERROR: no nodes found in .osm file")
        sys.exit(1)

    # Determine render bounds
    if args.bounds:
        lat_top, lon_left, lat_bot, lon_right = args.bounds
    else:
        lat_min, lon_min, lat_max, lon_max = osm.bounds()
        lat_top, lon_left, lat_bot, lon_right = lat_max, lon_min, lat_min, lon_max

    print(f"\nRender bounds:")
    print(f"  lat [{lat_top:.5f} → {lat_bot:.5f}]")
    print(f"  lon [{lon_left:.5f} → {lon_right:.5f}]")
    print()

    for zoom in args.zoom:
        tx0, ty0 = lat_lon_to_tile(lat_top, lon_left,  zoom)
        tx1, ty1 = lat_lon_to_tile(lat_bot, lon_right, zoom)
        tx0, tx1 = min(tx0, tx1), max(tx0, tx1)
        ty0, ty1 = min(ty0, ty1), max(ty0, ty1)

        total = (tx1 - tx0 + 1) * (ty1 - ty0 + 1)
        size_mb = total * TILE_PX * TILE_PX * 2 / 1024 / 1024
        print(f"Zoom {zoom}: tiles x[{tx0}–{tx1}] y[{ty0}–{ty1}] "
              f"= {total} tiles (~{size_mb:.1f} MB on SD)")

        done = 0
        for ty in range(ty0, ty1 + 1):
            for tx in range(tx0, tx1 + 1):
                tile_dir  = os.path.join(args.out, str(zoom), str(tx))
                tile_path = os.path.join(tile_dir, f"{ty}.bin")
                os.makedirs(tile_dir, exist_ok=True)

                img = render_tile(osm, tx, ty, zoom)
                raw = image_to_rgb565(img)
                with open(tile_path, "wb") as f:
                    f.write(raw)

                done += 1
                pct = done / total * 100
                print(f"\r  z={zoom} [{done}/{total}] {pct:.0f}%  ({tx},{ty})  ",
                      end="", flush=True)

        print(f"\r  z={zoom} done — {done} tiles written" + " " * 20)

    print()
    print("Done!")
    print(f"  Copy '{args.out}/' to your SD card as '/tiles/'")
    print()

    ctr_lat = (lat_top + lat_bot) / 2
    ctr_lon = (lon_left + lon_right) / 2
    print("Update AppConfig.h:")
    print(f"  mapSource  = 2")
    print(f"  mapZoom    = {args.zoom[-1]}   // highest zoom you rendered")
    print(f"  defaultLat = {ctr_lat:.6f}")
    print(f"  defaultLon = {ctr_lon:.6f}")


if __name__ == "__main__":
    main()
