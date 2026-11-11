#!/usr/bin/env python3
"""Generate a clearly synthetic image for background/watermark QA (Pillow)."""
import argparse
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output")
parser.add_argument("--font", default="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
args = parser.parse_args()
image = Image.new("RGB", (960, 600), "#f5f5f7")
draw = ImageDraw.Draw(image)
title, body, small = [ImageFont.truetype(args.font, size) for size in (30, 19, 15)]
draw.rectangle((0, 0, 960, 49), fill="white")
draw.line((0, 49, 960, 49), fill="#dfe1e7")
draw.text((26, 14), "Design notes", font=body, fill="#202126")
draw.text((710, 18), "Synthetic local fixture", font=small, fill="#747680")
draw.text((70, 94), "Overview", font=title, fill="#202126")
draw.text((70, 142), "Keep the essentials visible.", font=body, fill="#747680")
for index, (name, description) in enumerate((
    ("Capture", "Focus on what matters."), ("Edit", "Make the message clear."),
    ("Share", "Keep the original intact."),
)):
    x = 70 + index * 282
    draw.rounded_rectangle((x, 215, x + 256, 465), radius=16, fill="white", outline="#e3e5eb")
    draw.rounded_rectangle((x + 20, 239, x + 64, 283), radius=12,
                           fill=("#e6f0ff", "#eee9ff", "#e4f6ef")[index])
    draw.text((x + 25, 248), str(index + 1), font=body, fill="#007aff")
    draw.text((x + 20, 316), name, font=body, fill="#202126")
    draw.text((x + 20, 356), description, font=small, fill="#747680")
    for row, width in enumerate((205, 180, 150)):
        draw.rounded_rectangle((x + 20, 395 + row * 13, x + 20 + width, 399 + row * 13), radius=2, fill="#e5e7ed")
draw.text((70, 530), "Non-personal sample • generated for UI and pixel regression tests", font=small, fill="#747680")
image.save(args.output)
