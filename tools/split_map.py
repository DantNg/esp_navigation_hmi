#!/usr/bin/env python3
"""
split_map.py - Split a large map image into tiles for the ESP32 SD-card map viewer.

Splits the image into square RGB565 binary tiles and writes a config.txt with
the tile dimensions AND geographic bounds so the ESP32 can geo-reference them.

Usage:
    python split_map.py <input_image> <output_dir> --bounds LAT_TOP,LON_LEFT,LAT_BOTTOM,LON_RIGHT

Examples:
    python split_map.py city_map.png sd_card/map/ --bounds 21.05,105.80,21.00,105.90
    python split_map.py satellite.jpg sd_card/map/ --bounds 10.7,-74.1,10.6,-73.9 --tile-size 128

Output structure:
    output_dir/
        config.txt          - map dimensions, geographic bounds, tile size
        tile_000_000.bin    - Row 0, Col 0 (top-left), raw RGB565 little-endian
        tile_000_001.bin    - Row 0, Col 1
        ...

config.txt format (3 lines):
    <totalWidth>,<totalHeight>
    <latTop>,<lonLeft>,<latBottom>,<lonRight>
    <tileSize>

After running:
    1. Format an SD card as FAT32.
    2. Create a folder named "map" at the SD root.
    3. Copy ALL files from output_dir into /map/ on the SD card.
    4. In AppConfig.h set  useSdMap = true  (or save via NVS key "sdmap"=1).

Requirements:
    pip install Pillow
"""

import os
import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow library required. Install with: pip install Pillow")
    sys.exit(1)


def rgb888_to_rgb565_le(r: int, g: int, b: int) -> bytes:
    """Return 2-byte little-endian RGB565 word."""
    word = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return bytes([word & 0xFF, (word >> 8) & 0xFF])


def parse_bounds(s: str):
    """Parse 'latTop,lonLeft,latBottom,lonRight' from a string."""
    parts = [p.strip() for p in s.split(',')]
    if len(parts) != 4:
        raise argparse.ArgumentTypeError(
            "bounds must be 4 comma-separated floats: latTop,lonLeft,latBottom,lonRight"
        )
    try:
        return tuple(float(p) for p in parts)
    except ValueError:
        raise argparse.ArgumentTypeError("bounds values must be floating-point numbers")


def split_map(input_path: str, output_dir: str,
              bounds: tuple, tile_size: int = 256) -> None:
    lat_top, lon_left, lat_bottom, lon_right = bounds

    print(f"Loading image: {input_path}")
    img = Image.open(input_path).convert('RGB')
    w, h = img.size
    print(f"  Image size:  {w} x {h} pixels")
    print(f"  Bounds:      lat {lat_top:.6f} → {lat_bottom:.6f}  "
          f"lon {lon_left:.6f} → {lon_right:.6f}")

    cols = (w + tile_size - 1) // tile_size
    rows = (h + tile_size - 1) // tile_size
    total_tiles = cols * rows
    print(f"  Tile grid:   {cols} cols x {rows} rows = {total_tiles} tiles ({tile_size}px)")

    os.makedirs(output_dir, exist_ok=True)

    # Write config.txt
    config_path = os.path.join(output_dir, 'config.txt')
    with open(config_path, 'w') as f:
        f.write(f"{w},{h}\n")
        f.write(f"{lat_top},{lon_left},{lat_bottom},{lon_right}\n")
        f.write(f"{tile_size}\n")
    print(f"  Config:      {config_path}")

    print(f"\nSplitting tiles...")
    pixels = img.load()
    for row in range(rows):
        for col in range(cols):
            x0 = col * tile_size
            y0 = row * tile_size
            crop_w = min(tile_size, w - x0)
            crop_h = min(tile_size, h - y0)

            data = bytearray(tile_size * tile_size * 2)
            idx = 0
            for py in range(tile_size):
                src_y = y0 + py
                for px in range(tile_size):
                    src_x = x0 + px
                    if src_x < w and src_y < h:
                        r, g, b = pixels[src_x, src_y]
                    else:
                        r = g = b = 0   # black padding
                    data[idx:idx+2] = rgb888_to_rgb565_le(r, g, b)
                    idx += 2

            filename = f"tile_{row:03d}_{col:03d}.bin"
            filepath = os.path.join(output_dir, filename)
            with open(filepath, 'wb') as f:
                f.write(data)

            tile_num = row * cols + col + 1
            print(f"  [{tile_num:4d}/{total_tiles}] {filename}  ({crop_w}x{crop_h} px)")

    total_bytes = total_tiles * tile_size * tile_size * 2
    print(f"\n{'='*56}")
    print(f"Done!  {total_tiles} tiles, "
          f"{total_bytes:,} bytes ({total_bytes/1024/1024:.1f} MB)")
    print(f"Output: {os.path.abspath(output_dir)}")
    print(f"\nNext steps:")
    print(f"  1. Format SD card as FAT32")
    print(f"  2. Copy ALL files from '{output_dir}' into the /map/ folder on the SD card")
    print(f"  3. In AppConfig.h set  useSdMap = true")
    print(f"     (or write NVS key \"sdmap\"=1 to switch without recompiling)")


def main():
    parser = argparse.ArgumentParser(
        description='Split a map image into RGB565 tiles for the ESP32 SD-card viewer',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s city_map.png output/map/ --bounds 21.05,105.80,21.00,105.90
  %(prog)s satellite.jpg output/map/ --bounds 10.7,-74.1,10.6,-73.9 --tile-size 128

How to get bounds:
  Open the map area in Google Maps → right-click top-left corner → "What's here?"
  to get (latTop, lonLeft).  Repeat for bottom-right to get (latBottom, lonRight).
        """
    )
    parser.add_argument('input',  help='Input image (PNG, JPG, BMP, …)')
    parser.add_argument('output', help='Output directory for tile files and config.txt')
    parser.add_argument('--bounds', required=True, type=parse_bounds,
                        metavar='LAT_TOP,LON_LEFT,LAT_BOTTOM,LON_RIGHT',
                        help='Geographic bounding box of the source image')
    parser.add_argument('--tile-size', type=int, default=256,
                        help='Tile size in pixels (default: 256)')

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: input file not found: {args.input}")
        sys.exit(1)
    if args.tile_size < 32 or args.tile_size > 512:
        print(f"Error: tile-size must be between 32 and 512")
        sys.exit(1)

    split_map(args.input, args.output, args.bounds, args.tile_size)


if __name__ == '__main__':
    main()
