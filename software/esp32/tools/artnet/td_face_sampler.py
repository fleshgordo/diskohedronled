# TouchDesigner Script CHOP callbacks: video TOP -> 120 channels for the diskohedron.
#
# Network:  your content TOP -> Null TOP "null_video"
#           Script CHOP (Callbacks DAT = this file) -> DMX Out CHOP (Art-Net, 192.168.4.1, universe 0)
#
# Each face averages ~100 sample points of the TOP (same mapping as tools/sphere/video2lamp.py),
# converts RGB to RGBW and outputs t01r t01g t01b t01w ... t30w with values 0..255.
# Needs face_samples.json from pixelmap.py (tools/artnet/out/), see README.md.

import json
import os

import numpy as np

SOURCE_TOP = 'null_video'               # TOP with the content, relative to this DAT
SAMPLES_JSON = 'face_samples.json'      # absolute, or relative to the .toe project folder
MAPPING = 'polar'                       # 'polar' (square content), 'equirect' (360°), 'cylinder'
WHITE = 1.0                             # 0..1: how much of the common RGB goes to the white LED
SATURATION = 1.3                        # >1 boosts colours (30 pixels look washed out)
FLIP_V = True                           # TOP.numpyArray() starts at the BOTTOM row

_cache = {}


def load_samples(path, mapping):
    key = (path, mapping)
    if key not in _cache:
        with open(path) as f:
            _cache[key] = np.array(json.load(f)['mappings'][mapping], dtype=np.float32)  # (30, S, 2)
    return _cache[key]


def faces_rgbw(img, uv, white=WHITE, saturation=SATURATION, flip_v=FLIP_V):
    """img: float array (h, w, >=3) with values 0..1 -> (30, 4) RGBW 0..255."""
    h, w = img.shape[:2]
    px = np.clip((uv[..., 0] * w).astype(int), 0, w - 1)
    v = 1 - uv[..., 1] if flip_v else uv[..., 1]
    py = np.clip((v * h).astype(int), 0, h - 1)
    rgb = img[py, px, :3].mean(axis=1) * 255
    rgb = rgb * saturation + rgb.mean(axis=1, keepdims=True) * (1 - saturation)
    rgb = np.clip(rgb, 0, 255)
    wv = rgb.min(axis=1) * white
    out = np.empty((30, 4), np.float32)
    out[:, :3] = rgb - wv[:, None]
    out[:, 3] = wv
    return out


# ------------------------------------------------------------------ TouchDesigner callbacks
def onSetupParameters(scriptOp):
    return


def onPulse(par):
    return


def onCook(scriptOp):
    top = op(SOURCE_TOP)  # noqa: F821 - provided by TouchDesigner
    path = SAMPLES_JSON if os.path.isabs(SAMPLES_JSON) else os.path.join(project.folder, SAMPLES_JSON)  # noqa: F821
    scriptOp.clear()
    scriptOp.numSamples = 1
    if top is None:
        return
    vals = faces_rgbw(top.numpyArray(), load_samples(path, MAPPING))
    for t in range(30):
        for c, name in enumerate('rgbw'):
            scriptOp.appendChan(f't{t + 1:02d}{name}')[0] = float(vals[t, c])
