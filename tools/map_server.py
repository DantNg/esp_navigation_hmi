#!/usr/bin/env python3
"""
map_server.py - Offline satellite map tile server for the Lite Ground Station.

The ESP32 ground station asks this tool for a satellite image centered on the
drone whenever the drone nears the edge of the currently displayed map. The tool
serves imagery from a *local* (offline) store of XYZ tiles and returns a raw
RGB565 image the device blits straight to the screen — no on-device decoding.

Protocol
--------
  GET /map?lat=<f>&lon=<f>&w=<int>&h=<int>&zoom=<int>

  Response body (binary, little-endian):
    offset 0  : magic    char[4] = "MAP1"
    offset 4  : width     uint16
    offset 6  : height    uint16
    offset 8  : lat_top   float64   (north edge)
    offset 16 : lon_left  float64   (west edge)
    offset 24 : lat_bottom float64  (south edge)
    offset 32 : lon_right float64   (east edge)
    offset 40 : pixels    width*height * RGB565 (2 bytes/px, little-endian),
                row-major, top-left origin.

Imagery store (offline)
-----------------------
Standard slippy-map / XYZ tiles on disk:

    <imagery>/<z>/<x>/<y>.png   (or .jpg)

Pre-download the area you will fly over (many tools can do this, e.g. an
offline-tiles downloader pointed at an Esri World Imagery / satellite source),
then run this server pointing --imagery at that folder. If a tile is missing the
server fills that area gray, and if --imagery is omitted entirely it serves a
synthetic grid so the end-to-end pipeline is still testable.

Usage
-----
    pip install pillow numpy
    python map_server.py --imagery ./tiles --port 8080
    python map_server.py                      # synthetic grid (no real imagery)

Requirements: Pillow (required), numpy (optional, much faster RGB565 encode).
"""

import argparse
import math
import os
import struct
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Error: Pillow required. pip install pillow")
    sys.exit(1)

try:
    import numpy as np
    HAVE_NUMPY = True
except ImportError:
    HAVE_NUMPY = False

TILE = 256
MAGIC = b"MAP1"

# Set by main() from CLI args.
IMAGERY_DIR = None
TILE_EXTS = (".png", ".jpg", ".jpeg")


# --------------------------------------------------------------------------
# Web Mercator helpers (global pixel space at a given zoom)
# --------------------------------------------------------------------------
def lonlat_to_pixel(lon, lat, zoom):
    n = TILE * (2 ** zoom)
    x = (lon + 180.0) / 360.0 * n
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x, y


def pixel_to_lonlat(x, y, zoom):
    n = TILE * (2 ** zoom)
    lon = x / n * 360.0 - 180.0
    lat = math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * y / n))))
    return lon, lat


# --------------------------------------------------------------------------
# Imagery assembly
# --------------------------------------------------------------------------
def load_tile(z, x, y):
    if IMAGERY_DIR is None:
        return None
    for ext in TILE_EXTS:
        path = os.path.join(IMAGERY_DIR, str(z), str(x), f"{y}{ext}")
        if os.path.exists(path):
            try:
                return Image.open(path).convert("RGB")
            except Exception as exc:  # noqa: BLE001
                print(f"  ! failed to open {path}: {exc}")
                return None
    return None


def build_image(lat, lon, w, h, zoom):
    """Return (PIL RGB image w x h, bounds dict) centered on lat/lon."""
    cx, cy = lonlat_to_pixel(lon, lat, zoom)
    left = cx - w / 2.0
    top = cy - h / 2.0

    bounds_lon_left, bounds_lat_top = pixel_to_lonlat(left, top, zoom)
    bounds_lon_right, bounds_lat_bottom = pixel_to_lonlat(left + w, top + h, zoom)
    bounds = {
        "lat_top": bounds_lat_top,
        "lon_left": bounds_lon_left,
        "lat_bottom": bounds_lat_bottom,
        "lon_right": bounds_lon_right,
    }

    if IMAGERY_DIR is None:
        return synthetic_image(lat, lon, w, h, zoom), bounds

    canvas = Image.new("RGB", (w, h), (60, 60, 66))
    li, ti = int(math.floor(left)), int(math.floor(top))
    x0_tile, y0_tile = li // TILE, ti // TILE
    x1_tile = int(math.floor((left + w - 1))) // TILE
    y1_tile = int(math.floor((top + h - 1))) // TILE

    missing = 0
    for tx in range(x0_tile, x1_tile + 1):
        for ty in range(y0_tile, y1_tile + 1):
            tile = load_tile(zoom, tx, ty)
            px = tx * TILE - int(round(left))
            py = ty * TILE - int(round(top))
            if tile is None:
                missing += 1
                continue
            canvas.paste(tile, (px, py))
    if missing:
        print(f"  ({missing} tiles missing -> gray)")
    return canvas, bounds


def synthetic_image(lat, lon, w, h, zoom):
    """A grid + crosshair placeholder so the pipeline works without imagery."""
    img = Image.new("RGB", (w, h), (24, 43, 64))
    d = ImageDraw.Draw(img)
    step = 64
    for x in range(0, w, step):
        d.line([(x, 0), (x, h)], fill=(38, 62, 88), width=1)
    for y in range(0, h, step):
        d.line([(0, y), (w, y)], fill=(38, 62, 88), width=1)
    d.line([(w // 2, 0), (w // 2, h)], fill=(70, 110, 150), width=2)
    d.line([(0, h // 2), (w, h // 2)], fill=(70, 110, 150), width=2)
    d.text((8, 8), f"SYNTHETIC MAP  z{zoom}", fill=(180, 210, 235))
    d.text((8, 24), f"center {lat:.6f}, {lon:.6f}", fill=(180, 210, 235))
    return img


def to_rgb565_le(img):
    """Encode a PIL RGB image to little-endian RGB565 bytes (row-major)."""
    if HAVE_NUMPY:
        a = np.asarray(img, dtype=np.uint16)            # h x w x 3
        r = (a[:, :, 0] >> 3) & 0x1F
        g = (a[:, :, 1] >> 2) & 0x3F
        b = (a[:, :, 2] >> 3) & 0x1F
        rgb565 = (r << 11) | (g << 5) | b
        return rgb565.astype("<u2").tobytes()
    # Slow fallback
    w, h = img.size
    px = img.load()
    out = bytearray(w * h * 2)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            out[i] = v & 0xFF
            out[i + 1] = (v >> 8) & 0xFF
            i += 2
    return bytes(out)


# --------------------------------------------------------------------------
# HTTP
# --------------------------------------------------------------------------
class MapHandler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # quieter; we print our own lines

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path != "/map":
            self.send_error(404, "use /map")
            return
        q = parse_qs(parsed.query)
        try:
            lat = float(q["lat"][0])
            lon = float(q["lon"][0])
            w = int(q.get("w", ["800"])[0])
            h = int(q.get("h", ["480"])[0])
            zoom = int(q.get("zoom", ["17"])[0])
        except (KeyError, ValueError):
            self.send_error(400, "need lat, lon, [w, h, zoom]")
            return

        w = max(64, min(w, 2048))
        h = max(64, min(h, 2048))
        zoom = max(1, min(zoom, 21))

        print(f"/map lat={lat:.6f} lon={lon:.6f} {w}x{h} z{zoom}")
        img, b = build_image(lat, lon, w, h, zoom)
        pixels = to_rgb565_le(img)

        header = struct.pack(
            "<4sHHdddd", MAGIC, w, h,
            b["lat_top"], b["lon_left"], b["lat_bottom"], b["lon_right"],
        )
        body = header + pixels

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    global IMAGERY_DIR
    ap = argparse.ArgumentParser(description="Offline RGB565 satellite map server")
    ap.add_argument("--imagery", help="XYZ tile dir (<z>/<x>/<y>.png); "
                                       "omit for a synthetic grid")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    if args.imagery:
        IMAGERY_DIR = os.path.abspath(args.imagery)
        if not os.path.isdir(IMAGERY_DIR):
            print(f"Warning: imagery dir not found: {IMAGERY_DIR} (serving synthetic)")
            IMAGERY_DIR = None
        else:
            print(f"Imagery store: {IMAGERY_DIR}")
    else:
        print("No --imagery given: serving SYNTHETIC grid images.")
    if not HAVE_NUMPY:
        print("note: numpy not installed; RGB565 encode will be slow.")

    srv = ThreadingHTTPServer((args.host, args.port), MapHandler)
    print(f"Listening on http://{args.host}:{args.port}/map  (Ctrl+C to stop)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nbye")


if __name__ == "__main__":
    main()
