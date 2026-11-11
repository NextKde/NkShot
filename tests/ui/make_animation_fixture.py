#!/usr/bin/env python3
"""Create non-personal GIF/WebP fixtures; requires Pillow with WebP support."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("prefix", type=Path, help="Output path prefix without extension")
args = parser.parse_args()
frames = []
for index in range(12):
    frame = Image.new("RGB", (480, 270), "#f5f5f7")
    draw = ImageDraw.Draw(frame)
    draw.rounded_rectangle((24, 24, 456, 246), radius=16, fill="white")
    draw.text((44, 46), "Synthetic animation test", fill="#202126")
    draw.text((44, 70), f"Frame {index + 1:02d} / 12", fill="#707078")
    draw.ellipse((44 + index * 25, 130, 94 + index * 25, 180), fill="#007aff")
    frames.append(frame)
for extension in ("gif", "webp"):
    frames[0].save(str(args.prefix) + "." + extension, save_all=True,
                   append_images=frames[1:], duration=120, loop=0, lossless=True)
