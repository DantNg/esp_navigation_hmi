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
C_FARMLAND     = (238, 240, 213)   # farmland
C_SAND         = (245, 233, 198)   # sand/beach
C_SCHOOL       = (255, 245, 204)   # school/university/hospital grounds
C_PARKING      = (238, 237, 235)   # parking lots
C_CEMETERY     = (170, 203, 175)   # cemetery
C_RETAIL       = (255, 214, 209)   # retail area
C_AEROWAY      = (233, 209, 255)   # airport grounds
C_RAILWAY      = (120, 120, 120)   # railway lines

# Roads — drawn in order: fill colour, then an optional outline colour
ROAD_STYLES = {
    # (fill_rgb, width_px_at_z17, outline_rgb_or_None)
    "motorway"      : ((232, 146,  10), 6, (204, 128,   9)),
    "trunk"         : ((250, 178,  11), 5, (220, 157,  10)),
    "primary"       : ((252, 214,  95), 5, (210, 178,  80)),
    "secondary"     : ((253, 242, 144), 4, (200, 192, 116)),
    "tertiary"      : ((255, 255, 255), 4, (170, 170, 170)),
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

def normalize_highway(hw: str) -> str:
    """Map any highway value onto one of the ROAD_STYLES keys (or '')."""
    if not hw:
        return ""
    if hw in ROAD_STYLES:
        return hw
    # link roads (motorway_link, primary_link, …) render like their parent
    if hw.endswith("_link"):
        base = hw[:-5]
        if base in ROAD_STYLES:
            return base
    # everything else that is drivable/walkable gets a sensible fallback
    if hw in ("road", "busway", "bus_guideway", "construction", "raceway"):
        return "residential"
    if hw in ("bridleway", "corridor", "via_ferrata"):
        return "path"
    return ""

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

def stitch_rings(segments):
    """Join open way segments end-to-end into closed rings of node ids."""
    segs = [s for s in segments if len(s) >= 2]
    rings = []
    while segs:
        ring = segs.pop()
        progressed = True
        while ring[0] != ring[-1] and progressed:
            progressed = False
            for i, s in enumerate(segs):
                if s[0] == ring[-1]:
                    ring += s[1:]
                elif s[-1] == ring[-1]:
                    ring += s[-2::-1]
                elif s[-1] == ring[0]:
                    ring = s[:-1] + ring
                elif s[0] == ring[0]:
                    ring = s[::-1][:-1] + ring
                else:
                    continue
                segs.pop(i)
                progressed = True
                break
        if len(ring) >= 3:
            rings.append(ring)
    return rings

class OsmData:
    def __init__(self):
        self.nodes  = {}   # node_id -> (lat, lon)
        self.ways   = []   # list of {tags: dict, node_ids: list}
        self.ways_by_id = {}

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
                way = {"tags": tags, "node_ids": nrefs}
                self.ways.append(way)
                self.ways_by_id[int(elem.get("id"))] = way

        # Multipolygon relations: large lakes, rivers, parks, forests are
        # usually mapped as relations, not single ways. Stitch their outer
        # member ways into closed rings and add them as synthetic area ways.
        nrel = 0
        for elem in root.iter("relation"):
            tags = {t.get("k"): t.get("v") for t in elem.findall("tag")}
            if tags.get("type") not in ("multipolygon", "boundary"):
                continue
            outer_segs = []
            for m in elem.findall("member"):
                if m.get("type") != "way":
                    continue
                role = m.get("role", "")
                if role not in ("outer", ""):
                    continue
                w = self.ways_by_id.get(int(m.get("ref")))
                if w:
                    outer_segs.append(list(w["node_ids"]))
            for ring in stitch_rings(outer_segs):
                self.ways.append({"tags": tags, "node_ids": ring})
                nrel += 1

        # Precompute each way's bbox so tiles can skip far-away ways quickly
        for way in self.ways:
            lats = [self.nodes[n][0] for n in way["node_ids"] if n in self.nodes]
            lons = [self.nodes[n][1] for n in way["node_ids"] if n in self.nodes]
            way["bbox"] = (min(lats), min(lons), max(lats), max(lons)) if lats else None

        print(f"  {len(self.nodes):,} nodes, {len(self.ways):,} ways "
              f"({nrel} rings from relations)")

    def bounds(self):
        if not self.nodes:
            return None
        lats = [v[0] for v in self.nodes.values()]
        lons = [v[1] for v in self.nodes.values()]
        return min(lats), min(lons), max(lats), max(lons)

# ── tile renderer ─────────────────────────────────────────────────────────────

SS = 2   # supersampling factor — render at 512px, downscale to 256px (anti-aliasing)

def _scale(lat, lon, lat_top, lon_left, lat_bot, lon_right, px=TILE_PX):
    """Map lat/lon to pixel (float) within the tile."""
    x = (lon - lon_left)  / (lon_right  - lon_left)  * px
    y = (lat_top - lat)   / (lat_top    - lat_bot)   * px
    return x, y


def render_tile(osm: OsmData, tx: int, ty: int, zoom: int) -> Image.Image:
    lat_top, lon_left, lat_bot, lon_right = tile_bounds(tx, ty, zoom)
    px = TILE_PX * SS

    # Add a small margin so features near the edge don't get clipped
    dlat = lat_top - lat_bot
    dlon = lon_right - lon_left
    M = 0.05   # 5% margin
    lat_top_m  = lat_top  + dlat * M
    lat_bot_m  = lat_bot  - dlat * M
    lon_left_m = lon_left - dlon * M
    lon_right_m= lon_right+ dlon * M

    img  = Image.new("RGB", (px, px), C_BACKGROUND)
    draw = ImageDraw.Draw(img)

    def visible(way):
        bb = way.get("bbox")
        return bb and not (bb[2] < lat_bot_m or bb[0] > lat_top_m or
                           bb[3] < lon_left_m or bb[1] > lon_right_m)

    def coords(node_ids):
        pts = []
        for nid in node_ids:
            if nid not in osm.nodes:
                continue
            la, lo = osm.nodes[nid]
            x, y = _scale(la, lo, lat_top, lon_left, lat_bot, lon_right, px)
            pts.append((x, y))
        return pts

    # ── Layer 0: area fills (water, parks, buildings) ──────────────────────
    for way in osm.ways:
        if not visible(way):
            continue
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
        aeroway  = tags.get("aeroway", "")
        man_made = tags.get("man_made", "")

        if natural in ("water", "wetland", "bay", "strait") \
                or waterway in ("riverbank", "dock") \
                or landuse in ("reservoir", "basin") \
                or tags.get("water"):
            draw.polygon(pts, fill=C_WATER)
        elif natural in ("wood", "tree_row", "scrub", "heath"):
            draw.polygon(pts, fill=C_FOREST)
        elif landuse in ("forest", "orchard", "vineyard", "plant_nursery"):
            draw.polygon(pts, fill=C_FOREST)
        elif natural in ("sand", "beach", "shingle", "bare_rock"):
            draw.polygon(pts, fill=C_SAND)
        elif landuse in ("farmland", "farmyard", "greenhouse_horticulture", "allotments"):
            draw.polygon(pts, fill=C_FARMLAND)
        elif landuse in ("grass", "meadow", "village_green", "recreation_ground", "greenfield") \
                or natural in ("grassland",) \
                or leisure in ("park", "garden", "pitch", "playground", "golf_course",
                               "sports_centre", "stadium", "track", "common", "dog_park"):
            draw.polygon(pts, fill=C_PARK)
        elif landuse in ("cemetery", "grave_yard") or amenity == "grave_yard":
            draw.polygon(pts, fill=C_CEMETERY)
        elif amenity in ("school", "university", "college", "kindergarten",
                         "hospital", "clinic", "place_of_worship", "community_centre"):
            draw.polygon(pts, fill=C_SCHOOL, outline=(214, 204, 168))
        elif amenity == "parking":
            draw.polygon(pts, fill=C_PARKING, outline=(205, 203, 200))
        elif landuse == "residential" or landuse == "garages":
            draw.polygon(pts, fill=C_RESIDENTIAL)
        elif landuse == "retail":
            draw.polygon(pts, fill=C_RETAIL)
        elif landuse == "commercial":
            draw.polygon(pts, fill=C_COMMERCIAL)
        elif landuse in ("industrial", "railway", "construction", "brownfield",
                         "landfill", "quarry") or man_made == "works":
            draw.polygon(pts, fill=C_INDUSTRIAL)
        elif aeroway in ("aerodrome", "apron", "terminal", "helipad"):
            draw.polygon(pts, fill=C_AEROWAY)
        elif leisure in ("swimming_pool", "marina") :
            draw.polygon(pts, fill=C_WATER)
        elif tags.get("highway") == "pedestrian" and tags.get("area") == "yes":
            draw.polygon(pts, fill=ROAD_STYLES["pedestrian"][0])

    # ── Layer 0.5: buildings (always on top of landuse fills) ──────────────
    for way in osm.ways:
        if not way["tags"].get("building") or not visible(way):
            continue
        pts = coords(way["node_ids"])
        if len(pts) >= 3:
            draw.polygon(pts, fill=C_BUILDING, outline=C_BUILDING_OUT)

    # ── Layer 1: waterways (lines) ─────────────────────────────────────────
    for way in osm.ways:
        tags = way["tags"]
        ww = tags.get("waterway", "")
        if ww in ("river", "stream", "canal", "drain") and visible(way):
            pts = coords(way["node_ids"])
            if len(pts) >= 2:
                w = (3 if ww in ("river", "canal") else 1) * SS
                draw.line(pts, fill=C_WATER, width=w)

    # ── Layer 2: roads (back-to-front by importance) ───────────────────────
    scale_factor = max(0.5, 2 ** (zoom - 17))   # thinner roads at low zoom

    # group ways by normalized road type once (handles *_link and fallbacks)
    by_type = defaultdict(list)
    for way in osm.ways:
        tags = way["tags"]
        if tags.get("area") == "yes":
            continue   # pedestrian squares etc. already drawn as areas
        rt = normalize_highway(tags.get("highway", ""))
        if rt:
            by_type[rt].append(way)

    for road_type in ROAD_ORDER:
        fill_col, base_w, outline_col = ROAD_STYLES[road_type]
        w = max(1, round(base_w * scale_factor)) * SS

        for way in by_type.get(road_type, []):
            if not visible(way):
                continue
            pts = coords(way["node_ids"])
            if len(pts) < 2:
                continue
            if outline_col and w >= 2 * SS:
                draw.line(pts, fill=outline_col, width=w + 2 * SS)
            draw.line(pts, fill=fill_col, width=w)

    # ── Layer 3: railways (dashed dark line) ───────────────────────────────
    for way in osm.ways:
        rw = way["tags"].get("railway", "")
        if rw not in ("rail", "light_rail", "subway", "tram", "narrow_gauge"):
            continue
        if way["tags"].get("tunnel") == "yes" or not visible(way):
            continue
        pts = coords(way["node_ids"])
        if len(pts) >= 2:
            draw.line(pts, fill=C_RAILWAY, width=max(1, round(2 * scale_factor)) * SS)
            # white dashes on top to give the classic railway look
            for i in range(0, len(pts) - 1, 2):
                draw.line(pts[i:i + 2], fill=(255, 255, 255),
                          width=max(1, round(scale_factor)) * SS)

    if SS > 1:
        img = img.resize((TILE_PX, TILE_PX), Image.LANCZOS)
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
