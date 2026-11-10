#!/usr/bin/env python3
"""Synthetic, non-personal image for actual NkShot UI smoke captures."""
from pathlib import Path
import sys
texture = "--texture" in sys.argv[2:]
width, height = 960, 600
pixels = bytearray()
for y in range(height):
    for x in range(width):
        color = (30 + x * 90 // width, 80 + y * 100 // height, 150)
        if 90 <= x < 890 and 90 <= y < 510:
            color = (245, 247, 250)
        if 130 <= x < 430 and 140 <= y < 460:
            color = (45, 115, 200)
        if texture and 120 <= x < 680 and 100 <= y < 470:
            color = (220, 235, 250) if (x // 3 + y // 3) % 2 else (35, 75, 120)
        pixels.extend(color)
Path(sys.argv[1]).write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)
