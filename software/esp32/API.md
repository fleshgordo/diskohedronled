# HTTP JSON API

Control the lamp from Home Assistant, Homey, ioBroker, Node-RED, iOS Shortcuts or a script.
It's the same as the web page: changes show up there straight away, and the other way round.

Address: `http://diskoledron.local` or the lamp's IP. When the lamp has joined your home WiFi
the page shows the IP under "Home WiFi". Smart-home servers often can't resolve `.local`
names, so use the IP there and give the lamp a fixed address in your router.
On the lamp's own hotspot it is `http://192.168.4.1`.

No password: anyone on the same network can control the lamp.

## Endpoints

| Request | Does |
|---|---|
| `GET /api/state` | current state |
| `POST /api/state` + JSON body | change something, answers with the new state |
| `GET /api/state?on=1&brightness=40…` | the same with URL parameters, for tools that can only call a URL |
| `GET /api/schema` | all modes and parameters with ranges, plus `version`, `built`, `chip`, `appUsed` (% of firmware space) |
| `GET /api/presets` | preset names |

## State

```json
{
  "type": "state",
  "on": true,
  "mode": 5,
  "modeName": "Colour",
  "values": { "brightness": 65, "power": 1, "color.r": 60, "color.g": 20, "color.b": 0, "color.w": 255, "...": 0 },
  "manual": "ff000000ff000000…"
}
```

`values` holds every parameter (keys in the table below). `manual` holds the colours of Manual mode,
one `RRGGBBWW` hex group per board, board 1 first.

## Changing: POST body

Send only what you want to change. Header `Content-Type: application/json`.

| Field | Example | Meaning |
|---|---|---|
| `on` | `true`, `false`, `"toggle"` | lamp on/off. Off = dark, the mode is kept |
| `brightness` | `40` | 0–100 % |
| `preset` | `"Warm white"` | load a preset (exact name, see `/api/presets`) |
| `mode` | `"Ripples"` or `6` | switch mode, by name (any case) or number |
| `color` | `[255, 120, 0, 40]` | Colour mode with R, G, B, W (0–255; W may be left out) |
| `values` | `{"ripple.speed": 20}` | set any parameters |

`preset`, `mode` and `color` also switch the lamp on unless the request says `"on": false`.
If several fields are sent they're applied in this order: preset, mode, color, values, brightness, on.
An unknown mode or a bad `color` answers `400` with `{"error": "..."}`. An unknown preset name
is ignored.

## URL parameters

The same fields as URL parameters, for `GET` (or a form `POST`):

```
/api/state?on=0
/api/state?on=toggle
/api/state?preset=Warm%20white
/api/state?mode=Ripples&brightness=30
/api/state?color=255,0,0,0
/api/state?ripple.speed=20&ripple.rate=12      any parameter by its key
```

`on` accepts `1`/`0`, `true`/`false`, `on`/`off`, `toggle`.

## Modes and parameters

| # | Mode | Parameters (key: range) |
|---|---|---|
| – | all modes | `brightness` 0–100, `power` 0–1 |
| 0 | Off | – |
| 1 | Flicker | `flicker.level` 0–255, `flicker.rate` 10–500 ms |
| 2 | Colour noise | `noise.r` `noise.g` `noise.b` `noise.w` 0–255, `noise.rate` 10–500 ms |
| 3 | Game of Life | `life.fade` 100–10000 ms, `life.reseed` 5–600 s |
| 4 | Sweep | `sweep.level` 0–255, `sweep.speed` −100–100 |
| 5 | Colour | `color.r` `color.g` `color.b` `color.w` 0–255 |
| 6 | Ripples | `ripple.rate` 1–60 /min, `ripple.speed` 2–90 °/s, `ripple.width` 10–90 °, `ripple.sat` 0–100 % |
| 7 | Check | `check.time` 100–3000 ms, `check.r` `check.g` `check.b` `check.w` 0–1 |
| 8 | Manual | colours per board: set in the web page, read from `manual` |

Values outside the range are clipped. `/api/schema` always has the current list.

## Examples

```sh
L=http://diskoledron.local

curl $L/api/state
curl -X POST $L/api/state -H 'Content-Type: application/json' -d '{"on": false}'
curl -X POST $L/api/state -H 'Content-Type: application/json' -d '{"preset": "Slow rain", "brightness": 40}'
curl -X POST $L/api/state -H 'Content-Type: application/json' -d '{"color": [255, 80, 0, 0]}'
curl "$L/api/state?on=toggle"
```

### Home Assistant

A switch with on/off and state, plus a command for presets (`configuration.yaml`,
replace the IP):

```yaml
switch:
  - platform: rest
    name: Diskohedron
    resource: http://192.168.178.31/api/state
    body_on: '{"on": true}'
    body_off: '{"on": false}'
    is_on_template: "{{ value_json.on }}"
    headers:
      Content-Type: application/json

rest_command:
  diskohedron:
    url: http://192.168.178.31/api/state
    method: post
    content_type: application/json
    payload: "{{ body | tojson }}"
```

Use it in automations or scripts:

```yaml
- action: rest_command.diskohedron
  data:
    body: { "preset": "Warm white", "brightness": 50 }
```

### iOS Shortcuts

Action "Get contents of URL": URL `http://<lamp IP>/api/state`, method POST, request body JSON,
e.g. field `preset` = `Warm white`. Or simply open `http://<lamp IP>/api/state?on=toggle`.

## Notes

- Answers come after the lamp has applied the change (usually a few ms, at most 0.5 s).
- Not available over the API (web page only): saving or deleting presets, Manual colours per board,
  calibration, WiFi settings.
- MQTT / Home Assistant auto-discovery would be a separate addition (~30–50 KB firmware). Not
  built in yet.
