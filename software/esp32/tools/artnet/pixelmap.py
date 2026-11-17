#!/usr/bin/env python3
"""Where each face of the lamp sits in a video canvas - for Resolume / TouchDesigner.

    python3 pixelmap.py                         # 1920x1080, polar + equirect
    python3 pixelmap.py --width 1080 --height 1080 --map polar

Writes into ./out/:
  pixelmap_<map>_<W>x<H>.csv   tile, DMX channel, centre x/y and box per face (Resolume fixtures)
  guide_<map>_<W>x<H>.png      transparent overlay with face regions + numbers
  face_samples.json            sample points per face, used by td_face_sampler.py

The mapping is the same as tools/sphere/video2lamp.py (polar: centre of the
inscribed circle = top of the lamp, circle edge = bottom; equirect: full canvas).
Use the same --rotate / --mirror / --azimuth values you found with --calibrate.
"""
import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'sphere'))
from geometry import tile_geometry, ROWS  # noqa: E402
from video2lamp import face_directions, directions_to_pixels  # noqa: E402

OUT = HERE / 'out'
RING_COL = [(255, 179, 71), (255, 210, 127), (155, 211, 174), (127, 183, 230),
            (179, 157, 219), (244, 143, 177), (229, 115, 115)]


def rot_z(deg):
    a = np.radians(deg)
    return np.array([[np.cos(a), -np.sin(a), 0], [np.sin(a), np.cos(a), 0], [0, 0, 1]])


def centres(rotate, mirror, azimuth):
    geo = tile_geometry(rotate, mirror)
    return np.array([geo[t][0] for t in range(1, 31)]) @ rot_z(azimuth).T


def pixel_dirs(w, h, mapping):
    """unit direction for every pixel (h, w, 3) and a mask of pixels that map to the sphere."""
    y, x = np.mgrid[0:h, 0:w] + 0.5
    if mapping == 'polar':
        r = min(w, h) / 2 - 0.5
        dx, dy = x - w / 2, h / 2 - y
        rad = np.hypot(dx, dy) / r
        theta, lon = rad * np.pi, np.arctan2(dy, dx)
        mask = rad <= 1
    elif mapping == 'equirect':
        lon = x / w * 2 * np.pi - np.pi
        theta = y / h * np.pi
        mask = np.ones_like(x, bool)
    else:  # cylinder
        lon = x / w * 2 * np.pi - np.pi
        theta = np.arccos(np.clip(1 - 2 * y / h, -1, 1))
        mask = np.ones_like(x, bool)
    d = np.stack([np.sin(theta) * np.cos(lon), np.sin(theta) * np.sin(lon), np.cos(theta)], -1)
    return d, mask


def guide(w, h, mapping, c, path):
    # render at reduced size, then scale up (fast and smooth enough for a guide)
    s = min(1.0, 960 / max(w, h))
    sw, sh = max(1, int(w * s)), max(1, int(h * s))
    d, mask = pixel_dirs(sw, sh, mapping)
    face = np.argmax(d @ c.T, axis=-1)
    ring = {t - 1: r for r, row in enumerate(ROWS) for t in row}
    rgba = np.zeros((sh, sw, 4), np.uint8)
    for k in range(30):
        sel = (face == k) & mask
        rgba[sel, :3] = RING_COL[ring[k]]
        rgba[sel, 3] = 110
    edge = np.zeros_like(mask)
    edge[:, 1:] |= face[:, 1:] != face[:, :-1]
    edge[1:, :] |= face[1:, :] != face[:-1, :]
    rgba[edge & mask] = (255, 255, 255, 230)
    img = Image.fromarray(rgba, 'RGBA').resize((w, h), Image.NEAREST)
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc', max(14, h // 30))
    except OSError:
        font = ImageFont.load_default()
    py, px = directions_to_pixels(c[:, None, :], w, h, mapping)
    for k in range(30):
        draw.text((int(px[k, 0]), int(py[k, 0])), str(k + 1), fill=(255, 255, 255, 255), font=font,
                  anchor='mm', stroke_width=3, stroke_fill=(0, 0, 0, 255))
    img.save(path)


def table(w, h, mapping, args, path):
    dirs = face_directions(args.rotate, args.mirror, args.azimuth)
    py, px = directions_to_pixels(dirs, w, h, mapping)
    c = centres(args.rotate, args.mirror, args.azimuth)
    cy, cx = directions_to_pixels(c[:, None, :], w, h, mapping)
    with open(path, 'w', newline='') as f:
        wr = csv.writer(f)
        wr.writerow(['tile', 'dmx_channel', 'x', 'y', 'box_left', 'box_top', 'box_width', 'box_height'])
        for k in range(30):
            wr.writerow([k + 1, args.start + k * 4, int(cx[k, 0]), int(cy[k, 0]),
                         int(px[k].min()), int(py[k].min()),
                         int(px[k].max() - px[k].min()) + 1, int(py[k].max() - py[k].min()) + 1])


def samples_json(args, path):
    dirs = face_directions(args.rotate, args.mirror, args.azimuth)
    data = {'about': 'normalized sample points per face (u: 0=left, v: 0=top), tile 1 first',
            'rotate': args.rotate, 'mirror': args.mirror, 'azimuth': args.azimuth, 'mappings': {}}
    n = 10000
    for m in ('polar', 'equirect', 'cylinder'):
        py, px = directions_to_pixels(dirs, n, n, m)
        data['mappings'][m] = np.round(np.stack([(px + 0.5) / n, (py + 0.5) / n], -1), 4).tolist()
    path.write_text(json.dumps(data))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--width', type=int, default=1920)
    ap.add_argument('--height', type=int, default=1080)
    ap.add_argument('--map', choices=('polar', 'equirect', 'cylinder', 'both'), default='both')
    ap.add_argument('--start', type=int, default=1, help='DMX channel of tile 1 red (ARTNET_START_CHANNEL)')
    ap.add_argument('--rotate', type=int, default=0)
    ap.add_argument('--mirror', action='store_true')
    ap.add_argument('--azimuth', type=float, default=0)
    args = ap.parse_args()

    OUT.mkdir(exist_ok=True)
    maps = ('polar', 'equirect') if args.map == 'both' else (args.map,)
    c = centres(args.rotate, args.mirror, args.azimuth)
    for m in maps:
        tag = f'{m}_{args.width}x{args.height}'
        table(args.width, args.height, m, args, OUT / f'pixelmap_{tag}.csv')
        guide(args.width, args.height, m, c, OUT / f'guide_{tag}.png')
        print(f'wrote out/pixelmap_{tag}.csv, out/guide_{tag}.png')
    samples_json(args, OUT / 'face_samples.json')
    print('wrote out/face_samples.json')


if __name__ == '__main__':
    main()
