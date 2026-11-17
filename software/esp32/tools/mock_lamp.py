"""Fake lamp for working on the web UI without hardware.

Serves ../web and speaks the same WebSocket protocol as the firmware.
    pip install websockets
    python3 tools/mock_lamp.py        -> open http://127.0.0.1:8765
The parameter table below mirrors PARAMS[] in diskohedron/State.cpp.

HTTP API (see ../API.md): GET /api/state, /api/schema, /api/presets, and changes as URL
parameters (/api/state?on=toggle). POST bodies are not supported here, only on the lamp.
"""
import asyncio, json, sys, mimetypes
from urllib.parse import parse_qs
from pathlib import Path
from http import HTTPStatus
from websockets.asyncio.server import serve, broadcast
from websockets.http11 import Response
from websockets.datastructures import Headers

WEB = Path(__file__).resolve().parent.parent / "web"
MODES = ["Off", "Flicker", "Colour noise", "Game of Life", "Sweep", "Colour", "Ripples", "Check", "Manual"]
P = [("brightness",-1,"Brightness",0,100,100,"%",""),("power",-1,"Lamp on",0,1,1,"",""),("flicker.level",1,"Intensity",0,255,40,"","#f4f1e8"),
     ("flicker.rate",1,"Interval",10,500,40,"ms",""),("noise.r",2,"Red",0,255,255,"","#ff4b4b"),
     ("noise.g",2,"Green",0,255,255,"","#4bd36b"),("noise.b",2,"Blue",0,255,255,"","#4b7bff"),
     ("noise.w",2,"White",0,255,255,"","#f4f1e8"),("noise.rate",2,"Interval",10,500,40,"ms",""),
     ("life.fade",3,"Generation time",100,10000,3000,"ms",""),("life.reseed",3,"Reseed every",5,600,60,"s",""),
     ("sweep.level",4,"Intensity",0,255,255,"",""),("sweep.speed",4,"Speed",-100,100,30,"",""),
     ("color.r",5,"Red",0,255,0,"","#ff4b4b"),("color.g",5,"Green",0,255,0,"","#4bd36b"),
     ("color.b",5,"Blue",0,255,0,"","#4b7bff"),("color.w",5,"White",0,255,255,"","#f4f1e8"),
     ("ripple.rate",6,"Drops per minute",1,60,8,"",""),("ripple.speed",6,"Speed",2,90,12,"°/s",""),
     ("ripple.width",6,"Ring width",10,90,35,"°",""),("ripple.sat",6,"Colourfulness",0,100,80,"%",""),
     ("check.time",7,"Time per board",100,3000,500,"ms",""),("check.r",7,"Red",0,1,1,"","#ff4b4b"),
     ("check.g",7,"Green",0,1,1,"","#4bd36b"),("check.b",7,"Blue",0,1,1,"","#4b7bff"),("check.w",7,"White",0,1,1,"","#f4f1e8")]
schema = {"type":"schema","version":"1.1.0","built":"Nov 17 2026 15:02:11","chip":"ESP32-C3 (mock)","appUsed":64,"modes":[{"id":i,"name":n} for i,n in enumerate(MODES)],
          "params":[dict(key=k,mode=m,label=l,min=a,max=b,unit=u,color=c) for k,m,l,a,b,d,u,c in P]}
state = {"mode": 4, "values": {k: d for k,m,l,a,b,d,u,c in P}, "manual": "00000000" * 30}
presets = [{"name": "Flicker full", "mode": 1}, {"name": "Flicker low", "mode": 1}, {"name": "Krazy red", "mode": 2}, {"name": "Krazy green", "mode": 2}, {"name": "Krazy blue", "mode": 2}, {"name": "Redred", "mode": 2}, {"name": "Life slow", "mode": 3}, {"name": "Life fast", "mode": 3}, {"name": "Fast sweep", "mode": 4}, {"name": "Red", "mode": 5}, {"name": "Warm white", "mode": 5}, {"name": "Cold white", "mode": 5}, {"name": "Slow rain", "mode": 6}, {"name": "White drops", "mode": 6}, {"name": "Storm", "mode": 6}, {"name": "checklamp", "mode": 7}]  # = built-in presets in State.cpp
clients = set()
wifi = {"type":"wifi","ssid":"","status":"off","hostname":"diskoledron","scanning":False,"networks":[]}

calib = {"lux": [[0.0]*4 for _ in range(30)], "boards": True, "colours": True, "show": None}

def cj():
    """same rules as State::computeGains() in the firmware"""
    lux = calib["lux"]; gain = [[1000]*4 for _ in range(30)]; med = [0]*4
    for c in range(4):
        v = sorted(l[c] for l in lux if l[c] > 0)
        med[c] = v[len(v)//2] if v else 0
        used = [x for x in v if x >= 0.7*med[c]]
        for t in range(30):
            if calib["boards"] and lux[t][c] > 0 and lux[t][c] >= 0.7*med[c]: gain[t][c] = round(1000*used[0]/lux[t][c])
    bal = [1000]*4; W = [0.2126, 0.7152, 0.0722]
    if all(med[:3]):
        k = min(med[c]/W[c] for c in range(3)); bal[:3] = [round(1000*k*W[c]/med[c]) for c in range(3)]
    if calib["colours"]: gain = [[g[c]*bal[c]//1000 for c in range(4)] for g in gain]
    d = {"type":"calib","boards":calib["boards"],"colours":calib["colours"],"balance":bal,"lux":lux,"gain":gain}
    if calib["show"]: d["show"] = calib["show"]
    return json.dumps(d)

def st(): return json.dumps({"type":"state", "on": bool(state["values"]["power"]), "modeName": MODES[state["mode"]], **state})
def pr(): return json.dumps({"type":"presets","list":presets})
def wf(): return json.dumps(wifi)

async def fake_wifi(m):
    """wifi.scan -> fake networks; wifi.set -> connects, unless the password is "wrong"."""
    if m["type"] == "wifi.scan":
        wifi.update(scanning=True, networks=[]); broadcast(clients, wf()); await asyncio.sleep(2)
        wifi.update(scanning=False, networks=["Wohnung", "FRITZ!Box 7590 XY", "Nachbar 5G"])
    elif not m["ssid"]:
        wifi.update(ssid="", status="off"); wifi.pop("ip", None)
    else:
        wifi.update(ssid=m["ssid"], status="connecting"); wifi.pop("ip", None); broadcast(clients, wf())
        await asyncio.sleep(2)
        if m["pass"] == "wrong": wifi["status"] = "failed"
        else: wifi.update(status="connected", ip="192.168.1.42")
    broadcast(clients, wf())

async def handler(ws):
    clients.add(ws)
    await ws.send(json.dumps(schema)); await ws.send(pr()); await ws.send(st()); await ws.send(wf()); await ws.send(cj())
    frames = 0
    async for raw in ws:
        if isinstance(raw, bytes):  # video frame from sphere/video2lamp.py --stream
            frames += 1
            if len(raw) != 120: print("bad frame size", len(raw), flush=True)
            elif frames % 20 == 1: print(f"stream frame {frames}: tile1 RGBW={list(raw[:4])}", flush=True)
            continue
        m = json.loads(raw); print("<-", m, flush=True)
        if m["type"].startswith("wifi."): asyncio.create_task(fake_wifi(m)); continue
        if m["type"].startswith("calib."):
            cmd = m["type"][6:]
            if cmd == "show": calib["show"] = {"tile": m["tile"], "ch": m["ch"]}
            elif cmd == "stop": calib["show"] = None
            elif cmd == "lux": calib["lux"][m["tile"]][m["ch"]] = max(0.0, float(m["lux"]))
            elif cmd == "enable": calib.update(boards=m["boards"], colours=m["colours"])
            elif cmd == "reset":
                for l in calib["lux"]: l[m["ch"]] = 0.0
            broadcast(clients, cj()); continue
        if m["type"] == "mode": state["mode"] = m["mode"]
        elif m["type"] == "set": state["values"][m["key"]] = m["value"]
        elif m["type"] == "manual.set":
            h = state["manual"]
            for t in m["tiles"]: h = h[:t*8] + "".join(f"{v:02x}" for v in m["rgbw"]) + h[t*8+8:]
            state["manual"] = h
        elif m["type"] == "preset.save": presets.append({"name":m["name"],"mode":state["mode"]}); broadcast(clients, pr())
        elif m["type"] == "preset.delete":
            presets[:] = [p for p in presets if p["name"] != m["name"]]; broadcast(clients, pr())
        broadcast(clients, st())
    clients.discard(ws)

def api_json(code, obj):
    return Response(code, "OK" if code == 200 else "Bad Request",
                    Headers({"Content-Type": "application/json", "Access-Control-Allow-Origin": "*"}),
                    json.dumps(obj).encode())

def api_state(query):
    """URL parameters -> the same changes as on the lamp (Net.cpp apiMessages)"""
    q = {k: v[-1] for k, v in parse_qs(query).items()}
    keys = {k for k, *_ in P}
    turn_on = False
    if "preset" in q:
        p = next((p for p in presets if p["name"] == q["preset"]), None)
        if p: state["mode"] = p["mode"]
        turn_on = True
    if "mode" in q:
        m = q["mode"]
        mid = int(m) if m.lstrip("-").isdigit() else next((i for i, n in enumerate(MODES) if n.lower() == m.lower()), -1)
        if not 0 <= mid < len(MODES): return api_json(400, {"error": "unknown mode"})
        state["mode"] = mid; turn_on = True
    if "color" in q:
        c = [int(x) for x in q["color"].split(",")]
        if not 3 <= len(c) <= 4: return api_json(400, {"error": "color must be [r, g, b] or [r, g, b, w]"})
        c += [0] * (4 - len(c)); state["mode"] = 5; turn_on = True
        for k, v in zip(("color.r", "color.g", "color.b", "color.w"), c): state["values"][k] = v
    for k, v in q.items():
        if k in keys and k != "power" and v.lstrip("-").isdigit(): state["values"][k] = int(v)
    if "on" in q:
        v = q["on"]
        state["values"]["power"] = (1 - state["values"]["power"]) if v == "toggle" else int(v.lower() in ("1", "true", "on"))
    elif turn_on: state["values"]["power"] = 1
    if q: broadcast(clients, st())
    return api_json(200, json.loads(st()))

def http(conn, req):
    if req.path == "/ws": return None
    path, _, query = req.path.partition("?")
    if path == "/api/state": return api_state(query)
    if path == "/api/schema": return api_json(200, schema)
    if path == "/api/presets": return api_json(200, json.loads(pr()))
    f = WEB / (req.path.strip("/") or "index.html")
    if not f.is_file(): return conn.respond(HTTPStatus.NOT_FOUND, "nf\n")
    return Response(200, "OK", Headers({"Content-Type": mimetypes.guess_type(f.name)[0] or "text/plain"}), f.read_bytes())

async def main():
    async with serve(handler, "127.0.0.1", 8765, process_request=http):
        await asyncio.Future()
asyncio.run(main())
