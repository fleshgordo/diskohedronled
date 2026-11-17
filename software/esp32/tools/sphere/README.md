# Sphere tools

Map images and videos onto the 30 faces of the diskohedron.

| File | What it does |
|---|---|
| `geometry.py` | 3D position of every board (tile 1–30 = position in the LED chain). Run it to self-check. |
| `layout.py` | Draws `../../docs/tile_layout.png` (which board sits on which face). |
| `video2lamp.py` | Wraps an image/video onto the lamp: preview GIF, `.disko` frame file, or live stream. |
| `make_test_video.py` | Generates `test_colorwheel.mp4`. |
| `test_colorwheel.mp4` | Test video: a colour wheel turning once around the lamp, plus a white ring moving top → bottom twice. |
| `test_colorwheel_preview.gif` | What the lamp shows for that video (front and back). |
| `ripple_preview.py` / `ripple_preview.gif` | Preview of the firmware's Ripples mode (same algorithm). `--stream` plays it on the lamp. |
| `export_geometry.py` | Writes the face directions into the firmware (`TileGeometry.h`). |

Needs `numpy`, `opencv-python`, `matplotlib`, `websockets`.

## Quick start

```sh
python3 video2lamp.py test_colorwheel.mp4 --preview out.gif                  # preview
python3 video2lamp.py test_colorwheel.mp4 --stream ws://192.168.4.1/ws --loop  # on the lamp
python3 video2lamp.py --calibrate --stream ws://192.168.4.1/ws --loop          # find orientation
```

**Square videos** use `--map polar` (the default). The centre of the frame is
the top of the lamp, the edge of the circle is the bottom, and the angle around
the centre is the angle around the lamp. For 360° panoramas use
`--map equirect`.

**Calibration:**
- Expected: red = front, green 90° to the right of it, blue = back, white =
  left, dark top, yellow bottom.
- If green is on the left of red, add `--mirror`.
- `--azimuth <deg>` turns "front" to any direction.

See the main [README](../../README.md#video-on-the-lamp) for all options and the stream format.
