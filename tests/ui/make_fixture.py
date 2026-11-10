#!/usr/bin/env python3
"""Synthetic, non-personal image for actual NkShot UI smoke captures."""
from pathlib import Path
import sys
width, height = 960, 600
pixels = bytearray()
for y in range(height):
    for x in range(width):
        color = (30 + x * 90 // width, 80 + y * 100 // height, 150)
        if 90 <= x < 890 and 90 <= y < 510:
            color = (245, 247, 250)
        if 130 <= x < 430 and 140 <= y < 460:
            color = (45, 115, 200)
        pixels.extend(color)
Path(sys.argv[1]).write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)
