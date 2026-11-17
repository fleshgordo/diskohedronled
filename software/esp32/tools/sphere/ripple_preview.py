#!/usr/bin/env python3
"""Preview of the firmware's "Ripples" mode (same algorithm as Effects::ripple).

    python3 ripple_preview.py                       -> ripple_preview.gif
    python3 ripple_preview.py --rate 40 --speed 45 --width 20 --sat 100 -o storm.gif

Can also stream the simulation to the lamp: --stream ws://192.168.4.1/ws
"""
import argparse
import asyncio
import colorsys

import numpy as np

from geometry import tile_geometry
from video2lamp import write_preview, stream


def simulate(seconds, fps, rate, speed_deg, width_deg, sat, seed):
    rng = np.random.default_rng(seed)
    geo = tile_geometry()
    dirs = np.array([geo[t][0] for t in range(1, 31)])
    speed = np.radians(speed_deg)
    half = np.radians(width_deg) / 2
    drops = []                  # (dir, start, rgbw)
    next_drop = 0.0
    frames = []
    for i in range(int(seconds * fps)):
        now = i / fps
        if now >= next_drop:
            z = rng.uniform(-1, 1)
            phi = rng.uniform(0, 2 * np.pi)
            d = np.array([np.sqrt(1 - z * z) * np.cos(phi), np.sqrt(1 - z * z) * np.sin(phi), z])
            rgb = np.array(colorsys.hsv_to_rgb(rng.uniform(), sat / 100, 1))
            w = rgb.min()
            drops.append((d, now, np.append(rgb - w, w)))
            drops = drops[-12:]
            next_drop = now + max(0.3, rng.exponential(60 / rate))
        acc = np.zeros((30, 4))
        alive = []
        for d, start, col in drops:
            radius = speed * (now - start)
            if radius > np.pi + 2 * half:
                continue
            alive.append((d, start, col))
            amp = 1 - 0.6 * min(radius / np.pi, 1)
            if radius > np.pi:
                amp *= 1 - (radius - np.pi) / (2 * half)
            dist = np.arccos(np.clip(dirs @ d, -1, 1))
            x = (dist - radius) / half
            level = np.where(np.abs(x) < 3, amp * np.exp(-x * x), 0)
            acc += level[:, None] * col[None]
        drops = alive
        frames.append(np.clip(acc * 255, 0, 255).astype(np.uint8))
    return np.array(frames)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--rate', type=float, default=8, help='drops per minute')
    ap.add_argument('--speed', type=float, default=12, help='degrees per second')
    ap.add_argument('--width', type=float, default=35, help='ring width in degrees')
    ap.add_argument('--sat', type=float, default=80, help='colourfulness 0..100')
    ap.add_argument('--seconds', type=float, default=40)
    ap.add_argument('--fps', type=float, default=10)
    ap.add_argument('--seed', type=int, default=4)
    ap.add_argument('-o', '--out', default='ripple_preview.gif')
    ap.add_argument('--stream', metavar='WS_URL')
    args = ap.parse_args()

    frames = simulate(args.seconds, args.fps, args.rate, args.speed, args.width, args.sat, args.seed)
    write_preview(args.out, frames, args.fps, 0, False, 1)
    if args.stream:
        asyncio.run(stream(args.stream, frames, args.fps, True))


if __name__ == '__main__':
    main()
