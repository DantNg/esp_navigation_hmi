#!/usr/bin/env python3
"""
download_tiles.py  —  Download OSM (or any XYZ) tiles and convert to RGB565 binary.

Output layout (copy the entire <output_dir> to your SD card as /tiles):
    <output_dir>/<zoom>/<x>/<y>.bin   — 256×256 px, RGB565 little-endian

Usage examples
--------------
# Hanoi city centre, zoom 15 (coarse overview)
python download_tiles.py --bounds 21.09 105.77 20.99 105.87 --zoom 15 --out ./sd_tiles

# Zoom 17 (street level) — fewer tiles, smaller area
python download_tiles.py --bounds 21.082 105.780 21.068 105.796 --zoom 17 --out ./sd_tiles

# Use a different tile server (Esri satellite)
python download_tiles.py --bounds 21.082 105.780 21.068 105.796 --zoom 17 \\
    --server "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}" \\
    --out ./sd_tiles

Tile count warning
------------------
Zoom 15 → ~1-4 tiles/km²    (small files, wide area)
Zoom 17 → ~16-64 tiles/km²  (street detail, large number)
Zoom 18 → ~64-256 tiles/km² (very high detail, many files)

A 1×1 km area at zoom 17 is about 8 tiles across, 8 tiles tall = 64 tiles (8 MB).

Requirements
------------
    pip install requests Pillow
"""

import argparse
import math
import os
import struct
import sys
import time

try:
    import requests
    from PIL import Image
except ImportError:
    print("Missing dependencies. Install with:")
    print("    pip install requests Pillow")
    sys.exit(1)

DEFAULT_SERVER = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
USER_AGENT = "CrowPanelGroundStation/1.0 (offline map builder; educational use)"
REQUEST_DELAY = 0.5   # seconds between requests — respect OSM tile server ToS


def lat_lon_to_tile(lat: float, lon: float, zoom: int):
    """Return (tile_x, tile_y) for a lat/lon at a given zoom level."""
    n = 2 ** zoom
    x = int((lon + 180.0) / 360.0 * n)
    lat_r = math.radians(lat)
    y = int((1.0 - math.asinh(math.tan(lat_r)) / math.pi) / 2.0 * n)
    return x, y


def tile_to_lat_lon(tx: int, ty: int, zoom: int):
    """Return the lat/lon of the top-left corner of a tile."""
    n = 2 ** zoom
    lon = tx / n * 360.0 - 180.0
    lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * ty / n))))
    return lat, lon


def png_to_rgb565(img: Image.Image) -> bytes:
    """Convert a PIL Image to raw RGB565 little-endian bytes."""
    img = img.convert("RGB").resize((256, 256), Image.LANCZOS)
    pixels = img.load()
    w, h = img.size
    buf = bytearray(w * h * 2)
    idx = 0
    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            struct.pack_into("<H", buf, idx, rgb565)
            idx += 2
    return bytes(buf)


def download_tile(server: str, z: int, x: int, y: int,
                  session: requests.Session) -> Image.Image | None:
    """Download one tile and return a PIL Image, or None on error."""
    url = server.replace("{z}", str(z)).replace("{x}", str(x)).replace("{y}", str(y))
    try:
        resp = session.get(url, timeout=15)
        resp.raise_for_status()
        from io import BytesIO
        return Image.open(BytesIO(resp.content))
    except Exception as e:
        print(f"  WARN: failed to download {url}: {e}")
        return None


def main():
    parser = argparse.ArgumentParser(
        description="Download OSM tiles and convert to RGB565 binary for SD card.")
    parser.add_argument("--bounds", nargs=4, type=float, required=True,
                        metavar=("LAT_TOP", "LON_LEFT", "LAT_BOTTOM", "LON_RIGHT"),
                        help="Geographic bounding box")
    parser.add_argument("--zoom", "-z", type=int, default=15,
                        help="Tile zoom level (default: 15)")
    parser.add_argument("--out", "-o", default="./sd_tiles",
                        help="Output directory (copy this to SD as /tiles)")
    parser.add_argument("--server", default=DEFAULT_SERVER,
                        help="XYZ tile server URL template with {z}/{x}/{y}")
    parser.add_argument("--delay", type=float, default=REQUEST_DELAY,
                        help="Delay between tile requests in seconds (default: 0.5)")
    parser.add_argument("--force", action="store_true",
                        help="Re-download tiles that already exist")
    args = parser.parse_args()

    lat_top, lon_left, lat_bot, lon_right = args.bounds
    zoom = args.zoom
    out_dir = args.out

    # Tile range
    tx0, ty0 = lat_lon_to_tile(lat_top,  lon_left,  zoom)
    tx1, ty1 = lat_lon_to_tile(lat_bot,  lon_right, zoom)
    # Ensure correct ordering (ty0 < ty1 because y increases southward)
    tx0, tx1 = min(tx0, tx1), max(tx0, tx1)
    ty0, ty1 = min(ty0, ty1), max(ty0, ty1)

    total = (tx1 - tx0 + 1) * (ty1 - ty0 + 1)
    size_mb = total * 256 * 256 * 2 / 1024 / 1024

    print(f"Bounds  : lat [{lat_top:.4f} → {lat_bot:.4f}]  "
          f"lon [{lon_left:.4f} → {lon_right:.4f}]")
    print(f"Zoom    : {zoom}")
    print(f"Tiles   : x [{tx0}–{tx1}]  y [{ty0}–{ty1}]  total={total}")
    print(f"SD size : ~{size_mb:.1f} MB ({total} tiles × 128 KB)")
    print(f"Server  : {args.server}")
    print(f"Output  : {os.path.abspath(out_dir)}")
    print()

    if total > 500:
        print(f"WARNING: {total} tiles is a lot. Consider reducing the area or zoom level.")
        answer = input("Continue? [y/N] ").strip().lower()
        if answer != "y":
            sys.exit(0)

    session = requests.Session()
    session.headers.update({"User-Agent": USER_AGENT})

    done = 0
    skipped = 0
    failed = 0

    for ty in range(ty0, ty1 + 1):
        for tx in range(tx0, tx1 + 1):
            tile_dir = os.path.join(out_dir, str(zoom), str(tx))
            tile_path = os.path.join(tile_dir, f"{ty}.bin")

            if not args.force and os.path.exists(tile_path):
                skipped += 1
                done += 1
                continue

            os.makedirs(tile_dir, exist_ok=True)
            img = download_tile(args.server, zoom, tx, ty, session)
            if img is None:
                failed += 1
            else:
                raw = png_to_rgb565(img)
                with open(tile_path, "wb") as f:
                    f.write(raw)

            done += 1
            pct = done / total * 100
            print(f"\r  [{done}/{total}] {pct:.0f}%  tx={tx} ty={ty}  "
                  f"fail={failed}  skip={skipped}  ", end="", flush=True)

            if img is not None:
                time.sleep(args.delay)

    print()
    print()
    print("Done.")
    print(f"  Downloaded : {done - skipped - failed}")
    print(f"  Skipped    : {skipped} (already existed)")
    print(f"  Failed     : {failed}")
    print()
    print("Next steps:")
    print(f"  1. Copy the contents of '{out_dir}/' to your SD card as '/tiles/'")
    print(f"     SD card should have: /tiles/{zoom}/{tx0}/{ty0}.bin  etc.")
    print(f"  2. In AppConfig.h set:")
    print(f"       mapSource  = 2          // XYZ tiles")
    print(f"       mapZoom    = {zoom}")
    ctr_lat = (lat_top + lat_bot) / 2
    ctr_lon = (lon_left + lon_right) / 2
    print(f"       defaultLat = {ctr_lat:.6f}")
    print(f"       defaultLon = {ctr_lon:.6f}")


if __name__ == "__main__":
    main()
