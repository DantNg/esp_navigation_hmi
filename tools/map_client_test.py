#!/usr/bin/env python3
"""
map_client_test.py - Exercise map_server.py exactly like the device does.

Sends the same GET /map request the ESP32 sends, decodes the binary RGB565
response, prints the returned geographic bounds, and saves a PNG so you can
eyeball the imagery. Lets you validate the map tool end-to-end without the
hardware.

Usage
-----
    pip install pillow numpy
    python map_client_test.py --host 127.0.0.1 --port 8080 \
        --lat 21.0285 --lon 105.8048 --w 1024 --h 1024 --zoom 17 --out map.png
"""

import argparse
import struct
import sys
import urllib.request

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("Error: pillow + numpy required.  pip install pillow numpy")
    sys.exit(1)

HEADER = 40  # magic[4] + u16 w + u16 h + f64 x4


def main():
    ap = argparse.ArgumentParser(description="Test client for map_server.py")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--lat", type=float, default=21.028511)
    ap.add_argument("--lon", type=float, default=105.804817)
    ap.add_argument("--w", type=int, default=1024)
    ap.add_argument("--h", type=int, default=1024)
    ap.add_argument("--zoom", type=int, default=17)
    ap.add_argument("--out", default="map_test.png")
    args = ap.parse_args()

    url = (f"http://{args.host}:{args.port}/map?lat={args.lat}&lon={args.lon}"
           f"&w={args.w}&h={args.h}&zoom={args.zoom}")
    print(f"GET {url}")
    with urllib.request.urlopen(url, timeout=15) as r:
        body = r.read()

    if len(body) < HEADER:
        print(f"Response too short: {len(body)} bytes")
        sys.exit(1)

    magic, w, h, lat_top, lon_left, lat_bottom, lon_right = struct.unpack(
        "<4sHHdddd", body[:HEADER])
    if magic != b"MAP1":
        print(f"Bad magic: {magic!r}")
        sys.exit(1)

    expected = HEADER + w * h * 2
    print(f"header: {w}x{h}")
    print(f"bounds: lat [{lat_bottom:.6f} .. {lat_top:.6f}]  "
          f"lon [{lon_left:.6f} .. {lon_right:.6f}]")
    print(f"body: {len(body)} bytes (expected {expected})")
    if len(body) != expected:
        print("WARNING: body size mismatch")

    px = np.frombuffer(body[HEADER:HEADER + w * h * 2], dtype="<u2").reshape(h, w)
    r8 = ((px >> 11) & 0x1F) << 3
    g8 = ((px >> 5) & 0x3F) << 2
    b8 = (px & 0x1F) << 3
    img = Image.fromarray(np.dstack([r8, g8, b8]).astype("uint8"), "RGB")
    img.save(args.out)
    print(f"saved {args.out}")


if __name__ == "__main__":
    main()
