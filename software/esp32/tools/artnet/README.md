# Art-Net input: Resolume & TouchDesigner

The lamp listens for **Art-Net** on UDP 6454 and shows up as a node called
`diskoledron`. While data arrives it overrides the selected mode. Five
seconds after the last packet it goes back to the mode. Global brightness from
the web UI still applies.

| Setting | Value (`config.h`) |
|---|---|
| IP | `192.168.4.1` on the lamp's hotspot |
| Universe | 0 (net 0, subnet 0, universe 0) |
| Channels | 1–120: tile 1 R, G, B, W, tile 2 R, G, B, W, … tile 30 W |
| Tile *n* | starts at channel `4·(n−1)+1` |

Connect the computer to the `diskoledron` WiFi first. If the lamp has joined your
home WiFi (web UI → Home WiFi), use its address there instead. The page shows it.

## Files

| File | Use |
|---|---|
| `pixelmap.py` | Generates the files in `out/` for your canvas size and orientation. |
| `out/pixelmap_<map>_<W>x<H>.csv` | Per tile: DMX channel, centre x/y and box in the canvas, for placing Resolume fixtures. |
| `out/guide_<map>_<W>x<H>.png` | Transparent overlay with numbered face regions. Put it on top of your content to see what each face shows. |
| `out/face_samples.json` | Sample points for `td_face_sampler.py`. |
| `td_face_sampler.py` | TouchDesigner Script CHOP: video TOP → 120 channels. |
| `artnet_test.py` | `send` test patterns or `.disko` files to the lamp; `listen` = fake lamp for checking your setup without hardware. |

`out/` already contains 1920×1080 maps. Make others with
`python3 pixelmap.py --width 1080 --height 1080`. Pass the same `--mirror` /
`--azimuth` you found with `video2lamp.py --calibrate`.

**Which map:**
- `polar` (default) puts the top of the lamp in the centre of the canvas, the
  bottom on the edge of the inscribed circle, and the angle around the centre
  = the angle around the lamp. Corners are unused.
- `equirect` is for 360° / panorama content.

## Resolume Arena

(Menu names from Arena 7; they may differ slightly in other versions.)

1. **Preferences → DMX:** enable Art-Net output on the WiFi network interface.
2. **Output → Advanced Output → `+` → DMX Lumiverse.** Set it to Art-Net,
   universe 0, target `192.168.4.1` (or pick `diskoledron` once it's been
   discovered).
3. **Fixture Editor:** create a fixture with **1 pixel, RGBW, 4 channels**
   (R, G, B, W in that order).
4. Add **30** of these fixtures to the Lumiverse, at DMX addresses 1, 5, 9, …,
   117 (consecutive). Fixture *n* = tile *n*.
5. **Position them on the input map:** use `x`, `y` (centre) and the box
   from `out/pixelmap_polar_1920x1080.csv`. Each fixture's area is what the
   face averages. Load `guide_polar_1920x1080.png` on a top layer while
   placing, then mute it.
6. Play content. Square or circular content works best with the polar map.

**Tip:** if placing 30 fixtures by hand is too slow, use the TouchDesigner
route, or render your content to a file and use
`../sphere/video2lamp.py --stream`.

## TouchDesigner

1. Content → **Null TOP** named `null_video` (any size; square for `polar`).
2. Copy `out/face_samples.json` next to your `.toe` file.
3. Add a **Script CHOP**. Open its Callbacks DAT and replace the content
   with `td_face_sampler.py`. The settings at the top are `SOURCE_TOP`,
   `MAPPING`, `WHITE`, `SATURATION`.
4. Script CHOP → **DMX Out CHOP**:
   - Interface: **Art-Net**
   - Network Address: `192.168.4.1`
   - Net 0 / Subnet 0 / Universe 0
   - Active: on
5. The Script CHOP should show 120 channels `t01r … t30w` (0–255). Channel
   order = DMX order, so nothing else to map.

If the image appears upside down (top and bottom of the lamp swapped), set
`FLIP_V = False` in the script. `TOP.numpyArray()` normally starts at the
bottom row.

## Check without the lamp

```sh
python3 artnet_test.py listen        # fake lamp: answers ArtPoll, prints what arrives
```
Point Resolume/TouchDesigner at this computer's IP (or 127.0.0.1) and you'll
see e.g. `30 packets/s  lit tiles: 1:[255, 0, 0, 0] …`.

## Check the lamp without Resolume

```sh
python3 artnet_test.py send                       # white light runs through tiles 1..30
python3 artnet_test.py send --tile 1 --color 255,0,0,0
```
