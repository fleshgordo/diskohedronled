#!/usr/bin/env python3
"""Wrap an image or video onto the 30 faces of the diskohedron.

Every face averages the pixels that fall onto it, so the 30 boards show a very
low-resolution "sphere" version of the video.

    # preview only (animated GIF of the lamp from two sides)
    python3 video2lamp.py clip.mp4 --preview clip_preview.gif

    # write a frame file (for playback / later use)
    python3 video2lamp.py clip.mp4 --out clip.disko

    # play live on the lamp over WiFi (connected to the lamp's hotspot)
    python3 video2lamp.py clip.mp4 --stream ws://192.168.4.1/ws --loop

    # find --rotate / --mirror for your lamp (see README "Video on the lamp")
    python3 video2lamp.py --calibrate --stream ws://192.168.4.1/ws --loop

Projections (--map):
  polar     (default, made for SQUARE videos) the centre of the frame is the top
            of the lamp, the edge of the inscribed circle is the bottom; the angle
            around the centre is the angle around the lamp. Corners are unused.
  equirect  2:1 panorama / 360° video: x = longitude, y = latitude.
  cylinder  wrap the frame around the lamp like a label: x = around, y = height.

Output .disko file: b"DSKO", u8 version=1, u8 tiles=30, u16 fps*100, u32 frames,
then frames x 30 tiles x (R, G, B, W) bytes, tile 1 (first board) first.
"""
import argparse
import asyncio
import struct
import sys
import time

import numpy as np

from geometry import tile_geometry

SAMPLES = 10  # samples per face edge -> 100 directions per face


# ------------------------------------------------------------------ geometry -> sample directions
def face_directions(rotate, mirror, spin_deg=0.0):
    """(30, S, 3) unit vectors covering each face, tile 1 first."""
    geo = tile_geometry(rotate, mirror)
    u = (np.arange(SAMPLES) + 0.5) / SAMPLES
    uu, vv = np.meshgrid(u, u)
    uu, vv = uu.ravel(), vv.ravel()
    dirs = []
    for t in range(1, 31):
        c = geo[t][1]  # 4 corners in order -> bilinear patch
        p = (c[0][None] * ((1 - uu) * (1 - vv))[:, None] + c[1][None] * (uu * (1 - vv))[:, None]
             + c[2][None] * (uu * vv)[:, None] + c[3][None] * ((1 - uu) * vv)[:, None])
        dirs.append(p / np.linalg.norm(p, axis=1, keepdims=True))
    d = np.array(dirs)
    if spin_deg:
        a = np.radians(spin_deg)
        rz = np.array([[np.cos(a), -np.sin(a), 0], [np.sin(a), np.cos(a), 0], [0, 0, 1]])
        d = d @ rz.T
    return d


def directions_to_pixels(d, w, h, mapping):
    x, y, z = d[..., 0], d[..., 1], np.clip(d[..., 2], -1, 1)
    lon = np.arctan2(y, x)                 # -pi..pi, 0 = "front"
    if mapping == 'polar':
        theta = np.arccos(z)               # 0 at the top, pi at the bottom
        r = theta / np.pi * (min(w, h) / 2 - 0.5)
        px = w / 2 + r * np.cos(lon)
        py = h / 2 - r * np.sin(lon)
    elif mapping == 'equirect':
        px = (lon + np.pi) / (2 * np.pi) * w
        py = (np.pi / 2 - np.arcsin(z)) / np.pi * h
    elif mapping == 'cylinder':
        px = (lon + np.pi) / (2 * np.pi) * w
        py = (1 - z) / 2 * h
    else:
        raise ValueError(mapping)
    px = np.clip(px.astype(int), 0, w - 1)
    py = np.clip(py.astype(int), 0, h - 1)
    return py, px


# ------------------------------------------------------------------ colour
def to_rgbw(rgb, white):
    """rgb (30, 3) 0..255 -> (30, 4). The common part of R, G, B goes to the white LED."""
    w = rgb.min(axis=1) * white
    out = np.empty((rgb.shape[0], 4))
    out[:, :3] = rgb - w[:, None]
    out[:, 3] = w
    return out


def apply_gamma(v, gamma):
    return 255 * (v / 255) ** gamma


# ------------------------------------------------------------------ input
def calibration_frame(size=256):
    """Polar test image: around the lamp red (front, 0°), green (90°), blue (180°), white (270°);
    the top cap stays dark, the bottom cap is yellow."""
    yy, xx = np.mgrid[0:size, 0:size] - size / 2 + 0.5
    lon = np.degrees(np.arctan2(-yy, xx)) % 360
    theta = np.hypot(xx, yy) / (size / 2) * 180
    img = np.zeros((size, size, 3), np.uint8)
    quad = ((lon + 45) % 360 // 90).astype(int)
    colours = np.array([[255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 255]], np.uint8)
    img[:] = colours[quad]
    img[theta < 40] = 0
    img[theta > 140] = (255, 200, 0)
    return img


def read_frames(path, size, max_frames=None):
    """Yield (fps, frame RGB uint8 resized to size x size-ish). Images give one frame."""
    import cv2
    img = cv2.imread(path, cv2.IMREAD_COLOR)
    if img is not None:
        yield 1.0, cv2.cvtColor(_shrink(img, size), cv2.COLOR_BGR2RGB)
        return
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        sys.exit(f'cannot open {path}')
    fps = cap.get(cv2.CAP_PROP_FPS) or 25
    n = 0
    while True:
        ok, frame = cap.read()
        if not ok or (max_frames and n >= max_frames):
            break
        n += 1
        yield fps, cv2.cvtColor(_shrink(frame, size), cv2.COLOR_BGR2RGB)


def _shrink(img, size):
    import cv2
    h, w = img.shape[:2]
    s = size / max(h, w)
    return cv2.resize(img, (max(1, int(w * s)), max(1, int(h * s))), interpolation=cv2.INTER_AREA) if s < 1 else img


# ------------------------------------------------------------------ conversion
def convert(args):
    frames, fps = [], 25.0
    cache = {}
    source = [(1.0, calibration_frame())] if args.calibrate else read_frames(args.input, args.size, args.max_frames)
    for i, (fps, img) in enumerate(source):
        h, w = img.shape[:2]
        spin = args.azimuth + args.spin * i / fps
        key = (w, h, round(spin, 3))
        if key not in cache:
            if len(cache) > 4:
                cache.clear()
            cache[key] = directions_to_pixels(face_directions(args.rotate, args.mirror, spin), w, h, args.map)
        py, px = cache[key]
        rgb = img[py, px].astype(float).mean(axis=1)          # (30, 3)
        rgb = rgb * args.saturation + rgb.mean(axis=1, keepdims=True) * (1 - args.saturation)
        rgbw = to_rgbw(np.clip(rgb, 0, 255), args.white)
        rgbw = apply_gamma(rgbw, args.gamma) * args.brightness
        frames.append(np.clip(rgbw, 0, 255).astype(np.uint8))
    if args.fps:
        fps = args.fps
    print(f'{len(frames)} frames at {fps:.2f} fps')
    return np.array(frames), fps


def write_disko(path, frames, fps):
    with open(path, 'wb') as f:
        f.write(b'DSKO' + struct.pack('<BBHI', 1, 30, int(round(fps * 100)), len(frames)))
        f.write(frames.tobytes())
    print(f'wrote {path} ({len(frames) * 120 / 1024:.1f} KB)')


# ------------------------------------------------------------------ preview
def write_preview(path, frames, fps, rotate, mirror, every):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation, PillowWriter
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    geo = tile_geometry(rotate, mirror)
    polys = [geo[t][1] for t in range(1, 31)]
    shown = frames[::every]

    def colours(fr):
        rgbw = fr.astype(float)
        rgb = rgbw[:, :3] + rgbw[:, 3:4]  # white LED shown as white
        return np.clip(rgb / 255, 0, 1)

    fig = plt.figure(figsize=(8, 4.2), facecolor='#111')
    cols = []
    for i, (title, azim) in enumerate((('front (0°)', 0), ('back (180°)', 180))):
        ax = fig.add_subplot(1, 2, i + 1, projection='3d', facecolor='#111')
        pc = Poly3DCollection(polys, facecolors=colours(shown[0]), edgecolors='#000', linewidths=0.6)
        ax.add_collection3d(pc)
        ax.view_init(10, azim)
        ax.set_xlim(-1.2, 1.2); ax.set_ylim(-1.2, 1.2); ax.set_zlim(-1.2, 1.2)
        ax.set_box_aspect((1, 1, 1)); ax.set_axis_off()
        ax.set_title(title, color='#aaa', fontsize=10)
        cols.append(pc)

    def update(k):
        for pc in cols:
            pc.set_facecolor(colours(shown[k]))
        return cols

    anim = FuncAnimation(fig, update, frames=len(shown), blit=False)
    anim.save(path, writer=PillowWriter(fps=max(1, fps / every)))
    print(f'wrote {path} ({len(shown)} frames)')


# ------------------------------------------------------------------ streaming
async def stream(url, frames, fps, loop):
    from websockets.asyncio.client import connect
    async with connect(url) as ws:
        print(f'streaming to {url}, ctrl-c to stop')
        period = 1 / fps
        while True:
            t0 = time.perf_counter()
            for i, fr in enumerate(frames):
                await ws.send(fr.tobytes())        # 120 bytes: binary frame = stream mode
                wait = t0 + (i + 1) * period - time.perf_counter()
                if wait > 0:
                    await asyncio.sleep(wait)
            if not loop:
                break


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('input', nargs='?', help='video or image file')
    ap.add_argument('--calibrate', action='store_true', help='use a built-in orientation test pattern as input')
    ap.add_argument('--map', choices=('polar', 'equirect', 'cylinder'), default='polar')
    ap.add_argument('--out', help='write .disko frame file')
    ap.add_argument('--preview', help='write animated GIF preview')
    ap.add_argument('--preview-every', type=int, default=2, help='use every n-th frame in the GIF')
    ap.add_argument('--stream', metavar='WS_URL', help='e.g. ws://192.168.4.1/ws')
    ap.add_argument('--loop', action='store_true', help='repeat when streaming')
    ap.add_argument('--fps', type=float, help='override frame rate')
    ap.add_argument('--max-frames', type=int)
    ap.add_argument('--size', type=int, default=256, help='downscale frames to this size first')
    ap.add_argument('--spin', type=float, default=0, help='rotate the image around the lamp, degrees/s')
    ap.add_argument('--azimuth', type=float, default=0, help='turn the image around the lamp by this many degrees')
    ap.add_argument('--white', type=float, default=1.0, help='0..1: how much of the common RGB goes to W')
    ap.add_argument('--saturation', type=float, default=1.3, help='>1 boosts colours (30 pixels look washed out)')
    ap.add_argument('--gamma', type=float, default=1.0, help='>1 darkens mid tones')
    ap.add_argument('--brightness', type=float, default=1.0)
    ap.add_argument('--rotate', type=int, default=0, help='0..2: which way the lamp faces (120° steps)')
    ap.add_argument('--mirror', action='store_true', help='if the image appears mirrored on the lamp')
    args = ap.parse_args()
    if not args.input and not args.calibrate:
        ap.error('give an input file or --calibrate')
    if args.calibrate:
        args.map, args.saturation, args.white = 'polar', 1.0, 1.0

    frames, fps = convert(args)
    if args.out:
        write_disko(args.out, frames, fps)
    if args.preview:
        write_preview(args.preview, frames, fps, args.rotate, args.mirror, args.preview_every)
    if args.stream:
        try:
            asyncio.run(stream(args.stream, frames, fps, args.loop))
        except KeyboardInterrupt:
            pass
    if not (args.out or args.preview or args.stream):
        print('nothing to do: add --out, --preview and/or --stream')


if __name__ == '__main__':
    main()
