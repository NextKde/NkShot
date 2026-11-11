#!/usr/bin/env python3
"""Synthetic, non-personal color study for the actual image sampler window."""
import argparse
from PIL import Image, ImageDraw, ImageFont
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output')
parser.add_argument('--alpha', action='store_true')
args = parser.parse_args()
image = Image.new('RGBA', (960, 600), '#f5f5f7')
draw = ImageDraw.Draw(image)
font = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
title, body, small = [ImageFont.truetype(font, s) for s in (38, 22, 16)]
draw.text((55, 43), 'Color study', font=title, fill='#242428')
draw.text((57, 105), 'A small palette, considered closely.', font=body, fill='#777781')
for index, (name, color) in enumerate([('Terracotta', '#D9816B'), ('Sage', '#78A991'), ('Periwinkle', '#8A98CE')]):
    x = 55 + index * 290
    fill = color if not args.alpha else (*tuple(bytes.fromhex(color[1:])), 128)
    draw.rounded_rectangle((x, 180, x + 270, 465), radius=18, fill=fill)
    draw.text((x, 486), name, font=body, fill='#38383d')
    draw.text((x, 519), color + (' · 50% alpha' if args.alpha else ''), font=small, fill='#777781')
draw.text((57, 568), 'Synthetic fixture · local image pixels, no desktop capture', font=small, fill='#777781')
image.save(args.output)
