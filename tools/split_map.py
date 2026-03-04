#!/usr/bin/env python3
"""
split_map.py - Split a large map image into tiles for the ESP32 map viewer.

This tool prepares a large map image for use with the tile-based map system
on CrowPanel ESP32. It splits the image into 256x256 pixel tiles in RGB565
binary format, ready to be copied to an SD card.

Usage:
    python split_map.py <input_image> <output_dir> [--tile-size 256]

Examples:
    python split_map.py city_map.png sd_card/map/
    python split_map.py satellite.jpg sd_card/map/ --tile-size 128

Output structure:
    output_dir/
        config.txt          - Map dimensions (width,height)
        tile_000_000.bin    - Row 0, Col 0 (top-left)
        tile_000_001.bin    - Row 0, Col 1
        tile_001_000.bin    - Row 1, Col 0
        ...

Requirements:
    pip install Pillow

After running this script, copy the output folder to the SD card as /map/
"""

import os
import sys
import struct
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow library required. Install with: pip install Pillow")
    sys.exit(1)


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    """Convert RGB888 color to RGB565 (as used by LVGL with LV_COLOR_16_SWAP=0)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def split_map(input_path: str, output_dir: str, tile_size: int = 256) -> None:
    """
    Split a large map image into RGB565 binary tiles.

    Args:
        input_path:  Path to the source map image (PNG, JPG, etc.)
        output_dir:  Directory to write tile files and config
        tile_size:   Width and height of each tile in pixels
    """
    print(f"Loading image: {input_path}")
    img = Image.open(input_path).convert('RGB')
    w, h = img.size
    print(f"  Image size: {w} x {h} pixels")

    cols = (w + tile_size - 1) // tile_size
    rows = (h + tile_size - 1) // tile_size
    total_tiles = cols * rows
    print(f"  Tile grid:  {cols} cols x {rows} rows = {total_tiles} tiles")
    print(f"  Tile size:  {tile_size} x {tile_size} px")

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Write config file
    config_path = os.path.join(output_dir, 'config.txt')
    with open(config_path, 'w') as f:
        f.write(f"{w},{h}\n")
    print(f"  Config written: {config_path}")

    # Process each tile
    print(f"\nSplitting into tiles...")
    for row in range(rows):
        for col in range(cols):
            # Calculate crop region
            x = col * tile_size
            y = row * tile_size
            crop_w = min(tile_size, w - x)
            crop_h = min(tile_size, h - y)

            # Create tile image (black-padded at edges)
            tile = Image.new('RGB', (tile_size, tile_size), (0, 0, 0))
            region = img.crop((x, y, x + crop_w, y + crop_h))
            tile.paste(region, (0, 0))

            # Convert to RGB565 binary (little-endian, matching ESP32)
            pixels = tile.load()
            data = bytearray(tile_size * tile_size * 2)
            idx = 0
            for py in range(tile_size):
                for px in range(tile_size):
                    r, g, b = pixels[px, py]
                    rgb565 = rgb888_to_rgb565(r, g, b)
                    # Little-endian byte order
                    data[idx] = rgb565 & 0xFF
                    data[idx + 1] = (rgb565 >> 8) & 0xFF
                    idx += 2

            # Write tile binary file
            filename = f"tile_{row:03d}_{col:03d}.bin"
            filepath = os.path.join(output_dir, filename)
            with open(filepath, 'wb') as f:
                f.write(data)

            tile_num = row * cols + col + 1
            print(f"  [{tile_num:4d}/{total_tiles}] {filename} "
                  f"({crop_w}x{crop_h} px)")

    # Summary
    total_bytes = total_tiles * tile_size * tile_size * 2
    print(f"\n{'='*50}")
    print(f"Done! Generated {total_tiles} tiles")
    print(f"Total tile data: {total_bytes:,} bytes ({total_bytes/1024/1024:.1f} MB)")
    print(f"Output directory: {os.path.abspath(output_dir)}")
    print(f"\nNext steps:")
    print(f"  1. Format an SD card as FAT32")
    print(f"  2. Create a 'map' folder on the SD card root")
    print(f"  3. Copy ALL files from '{output_dir}' into the 'map' folder")
    print(f"  4. Insert SD card into CrowPanel")
    print(f"  5. The map system will auto-detect and load the tiles")


def main():
    parser = argparse.ArgumentParser(
        description='Split a large map image into tiles for ESP32 map viewer',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s city_map.png output/map/
  %(prog)s satellite.jpg output/map/ --tile-size 128
  %(prog)s big_map.png output/map/ --tile-size 512
        """
    )
    parser.add_argument('input', help='Input map image (PNG, JPG, BMP, etc.)')
    parser.add_argument('output', help='Output directory for tile files')
    parser.add_argument('--tile-size', type=int, default=256,
                        help='Tile size in pixels (default: 256)')

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)

    if args.tile_size < 32 or args.tile_size > 1024:
        print(f"Error: Tile size must be between 32 and 1024")
        sys.exit(1)

    split_map(args.input, args.output, args.tile_size)


if __name__ == '__main__':
    main()
