#!/usr/bin/env python3
"""Square test video for video2lamp.py (polar mapping):
a colour wheel turning around the lamp, plus a white ring that travels
from the centre (= top of the lamp) to the edge (= bottom).

    python3 make_test_video.py            -> test_colorwheel.mp4
"""
from pathlib import Path
import cv2
import numpy as np

OUT = Path(__file__).resolve().parent / 'test_colorwheel.mp4'
SIZE, FPS, SECONDS = 256, 20, 6


def main():
    n = FPS * SECONDS
    vw = cv2.VideoWriter(str(OUT), cv2.VideoWriter_fourcc(*'mp4v'), FPS, (SIZE, SIZE))
    yy, xx = np.mgrid[0:SIZE, 0:SIZE] - SIZE / 2
    angle = np.degrees(np.arctan2(-yy, xx)) % 360
    radius = np.hypot(xx, yy) / (SIZE / 2)
    for i in range(n):
        hsv = np.zeros((SIZE, SIZE, 3), np.uint8)
        hsv[..., 0] = ((angle + i * 360 / n) % 360 / 2).astype(np.uint8)  # one turn per video
        hsv[..., 1] = 255
        hsv[..., 2] = 255
        img = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)
        ring = (i % (n // 2)) / (n // 2)                                # two passes per video
        img[np.abs(radius - ring) < 0.1] = 255
        img[radius > 1] = 0
        vw.write(img)
    vw.release()
    print(f'wrote {OUT.name}: {SIZE}x{SIZE}, {n} frames at {FPS} fps')


if __name__ == '__main__':
    main()
