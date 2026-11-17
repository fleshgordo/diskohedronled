#!/usr/bin/env python3
"""Art-Net test tool for the diskohedron.

Send to the lamp:
    python3 artnet_test.py send                      # chase: tiles 1..30 one after another
    python3 artnet_test.py send --tile 7 --color 255,0,0,0
    python3 artnet_test.py send --file clip.disko    # play a file from sphere/video2lamp.py

Pretend to be the lamp (check your Resolume / TouchDesigner output without hardware):
    python3 artnet_test.py listen

Defaults: lamp at 192.168.4.1, universe 0, channels start at 1 - same as config.h.
"""
import argparse
import socket
import struct
import time

PORT = 6454
ID = b'Art-Net\x00'


def art_dmx(universe, data, seq):
    data = bytes(data)
    if len(data) % 2:
        data += b'\x00'
    return ID + struct.pack('<H', 0x5000) + bytes([0, 14, seq, 0, universe & 0xff, (universe >> 8) & 0x7f]) \
        + struct.pack('>H', len(data)) + data


def art_poll():
    return ID + struct.pack('<H', 0x2000) + bytes([0, 14, 0, 0])


def poll_reply(ip, universe, name='diskoledron'):
    r = bytearray(239)
    r[0:8] = ID
    r[8:10] = struct.pack('<H', 0x2100)
    r[10:14] = socket.inet_aton(ip)
    r[14:16] = struct.pack('<H', PORT)
    r[17] = 1
    r[18], r[19] = (universe >> 8) & 0x7f, (universe >> 4) & 0x0f
    r[21], r[23] = 0xff, 0xc0
    r[26:26 + len(name)] = name.encode()
    long = b'Diskohedron LED (fake, artnet_test.py)'
    r[44:44 + len(long)] = long
    r[173], r[174], r[182], r[190] = 1, 0x80, 0x80, universe & 0x0f
    r[207:211] = socket.inet_aton(ip)
    r[211], r[212] = 1, 0x08
    return bytes(r)


def sock(bind=False):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, 'SO_REUSEPORT'):
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    if bind:
        s.bind(('', PORT))
    return s


# ------------------------------------------------------------------ send
def frames_chase(color):
    while True:
        for t in range(30):
            f = bytearray(120)
            f[t * 4:t * 4 + 4] = bytes(color)
            yield f


def frames_file(path):
    raw = open(path, 'rb').read()
    assert raw[:4] == b'DSKO', 'not a .disko file'
    _, tiles, fps100, n = struct.unpack('<BBHI', raw[4:12])
    body = raw[12:]
    while True:
        for i in range(n):
            yield body[i * 120:(i + 1) * 120], fps100 / 100


def cmd_send(a):
    s = sock()
    color = [int(x) for x in a.color.split(',')]
    seq = 1
    if a.file:
        gen = frames_file(a.file)
    elif a.tile:
        f = bytearray(120)
        f[(a.tile - 1) * 4:(a.tile - 1) * 4 + 4] = bytes(color)
        gen = ((f, 10.0) for _ in iter(int, 1))
    else:
        gen = ((f, a.speed) for f in frames_chase(color))
    print(f'sending universe {a.universe} to {a.ip}:{PORT}, ctrl-c to stop')
    try:
        for frame, fps in gen:
            dmx = bytes(a.start - 1) + bytes(frame)
            s.sendto(art_dmx(a.universe, dmx, seq), (a.ip, PORT))
            seq = seq % 255 + 1
            time.sleep(1 / fps)
    except KeyboardInterrupt:
        s.sendto(art_dmx(a.universe, bytes(a.start - 1 + 120), seq), (a.ip, PORT))  # all off


# ------------------------------------------------------------------ listen
def cmd_listen(a):
    s = sock(bind=True)
    s.settimeout(0.5)
    own_ip = socket.gethostbyname(socket.gethostname())
    print(f'fake lamp listening on UDP {PORT}, universe {a.universe}, channels {a.start}..{a.start + 119}')
    count, last, frame = 0, time.time(), None
    while True:
        try:
            data, peer = s.recvfrom(1024)
        except socket.timeout:
            data = None
        if data and data[:8] == ID:
            op = struct.unpack('<H', data[8:10])[0]
            if op == 0x2000:
                s.sendto(poll_reply(own_ip, a.universe), (peer[0], PORT))
                print(f'ArtPoll from {peer[0]} -> replied')
            elif op == 0x5000 and len(data) >= 18:
                uni = data[14] | ((data[15] & 0x7f) << 8)
                length = struct.unpack('>H', data[16:18])[0]
                if uni == a.universe:
                    dmx = data[18:18 + length]
                    frame = dmx[a.start - 1:a.start - 1 + 120].ljust(120, b'\x00')
                    count += 1
                else:
                    print(f'ignored universe {uni} from {peer[0]}')
        if time.time() - last >= 1:
            if count:
                lit = [f'{t + 1}:{list(frame[t * 4:t * 4 + 4])}' for t in range(30) if any(frame[t * 4:t * 4 + 4])]
                print(f'{count} packets/s  lit tiles: {" ".join(lit[:8])}{" ..." if len(lit) > 8 else ""}')
            count, last = 0, time.time()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('send', 'listen'):
        p = sub.add_parser(name)
        p.add_argument('--universe', type=int, default=0)
        p.add_argument('--start', type=int, default=1, help='DMX channel of tile 1 red')
    p = sub.choices['send']
    p.add_argument('--ip', default='192.168.4.1')
    p.add_argument('--tile', type=int, help='light only this tile')
    p.add_argument('--color', default='0,0,0,255', help='R,G,B,W')
    p.add_argument('--speed', type=float, default=4, help='chase steps per second')
    p.add_argument('--file', help='.disko file to play')
    a = ap.parse_args()
    cmd_send(a) if a.cmd == 'send' else cmd_listen(a)


if __name__ == '__main__':
    main()
