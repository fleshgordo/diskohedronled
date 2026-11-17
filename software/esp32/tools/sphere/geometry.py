"""Geometry of the diskohedron: which board (tile) sits where on the rhombic triacontahedron.

Sources (old firmware, diskoledronBasel.ino):
  NEIGHBOURS  - the 4 neighbour faces of every tile (initNeighbours)
  ROWS        - 7 horizontal rings from top to bottom (ledrows36)
  (the old table had tile 17 -> 5, which must be 17 -> 4; fixed here)
Tile numbers are 1-based and equal the position in the LED chain (tile 1 = first board).

The face graph of a rhombic triacontahedron is the icosidodecahedron graph. We build the
real polyhedron, put a 3-fold corner on top (rings of 3,6,3,6,3,6,3 faces) and search the
mapping tile -> face that respects both the neighbours and the rings.
"""
import itertools
import numpy as np

NEIGHBOURS = {
    1: (2, 3, 9, 10), 2: (3, 1, 14, 15), 3: (4, 5, 1, 2), 4: (5, 3, 17, 18), 5: (6, 7, 3, 4),
    6: (7, 5, 20, 21), 7: (8, 9, 5, 6), 8: (9, 7, 23, 24), 9: (10, 1, 7, 8), 10: (11, 12, 1, 9),
    11: (12, 10, 24, 25), 12: (13, 14, 10, 11), 13: (14, 12, 27, 28), 14: (15, 2, 12, 13), 15: (16, 17, 2, 14),
    16: (17, 15, 28, 29), 17: (18, 4, 15, 16), 18: (19, 20, 4, 17), 19: (20, 18, 29, 30), 20: (21, 6, 18, 19),
    21: (22, 23, 6, 20), 22: (23, 21, 30, 26), 23: (24, 8, 21, 22), 24: (25, 11, 8, 23), 25: (26, 27, 11, 24),
    26: (27, 25, 22, 30), 27: (28, 13, 25, 26), 28: (29, 16, 13, 27), 29: (30, 19, 16, 28), 30: (26, 22, 19, 29),
}
ROWS = [
    (1, 2, 3), (4, 5, 9, 10, 14, 15), (7, 12, 17), (6, 8, 11, 13, 16, 18),
    (20, 24, 28), (19, 21, 23, 25, 27, 29), (22, 26, 30),
]

PHI = (1 + 5 ** 0.5) / 2


def _icosahedron():
    v = []
    for a, b in itertools.product((-1, 1), repeat=2):
        v += [(0, a, b * PHI), (a, b * PHI, 0), (b * PHI, 0, a)]
    v = np.array(v, float)
    edges = [(i, j) for i in range(12) for j in range(i + 1, 12) if abs(np.linalg.norm(v[i] - v[j]) - 2) < 1e-6]
    tris = [t for t in itertools.combinations(range(12), 3)
            if all(abs(np.linalg.norm(v[a] - v[b]) - 2) < 1e-6 for a, b in itertools.combinations(t, 2))]
    return v, edges, tris


def _rotation_to_z(axis):
    a = axis / np.linalg.norm(axis)
    z = np.array([0, 0, 1.0])
    c = np.cross(a, z)
    s, d = np.linalg.norm(c), a @ z
    if s < 1e-9:
        return np.eye(3) if d > 0 else np.diag([1, -1, -1.0])
    k = np.array([[0, -c[2], c[1]], [c[2], 0, -c[0]], [-c[1], c[0], 0]]) / s
    ang = np.arctan2(s, d)
    return np.eye(3) + np.sin(ang) * k + (1 - np.cos(ang)) * k @ k


def build_faces():
    """30 rhombic faces: dict with 'center' (unit normal) and 'corners' (4x3, on the polyhedron)."""
    v, edges, tris = _icosahedron()
    # dual corners: centres of the icosahedron triangles, scaled so rhombi are planar
    tri_c = {t: v[list(t)].mean(0) for t in tris}
    # put one 3-fold corner (a triangle centre) on top
    R = _rotation_to_z(tri_c[tris[0]])
    faces = []
    for (i, j) in edges:
        ts = [t for t in tris if i in t and j in t]
        mid = (v[i] + v[j]) / 2
        n = mid / np.linalg.norm(mid)
        # scale every corner onto the face plane n.x = |mid|
        pts = [v[i], tri_c[ts[0]], v[j], tri_c[ts[1]]]
        pts = [p * (np.linalg.norm(mid) / (p @ n)) for p in pts]
        faces.append(dict(edge=(i, j), center=R @ n, corners=np.array([R @ p for p in pts])))
    # adjacency: edges sharing a vertex and lying on a common triangle
    for a, fa in enumerate(faces):
        fa['adj'] = set()
        for b, fb in enumerate(faces):
            if a != b and set(fa['edge']) & set(fb['edge']) and \
                    any(set(fa['edge']) | set(fb['edge']) == set(t) for t in tris):
                fa['adj'].add(b)
    return faces


def match_tiles(faces):
    """All mappings tile -> face index that respect neighbours and rings."""
    z = np.array([f['center'][2] for f in faces])
    levels = sorted(set(np.round(z, 6)), reverse=True)
    face_row = {k: levels.index(round(z[k], 6)) for k in range(30)}
    tile_row = {t: r for r, row in enumerate(ROWS) for t in row}
    assert [sum(1 for k in face_row if face_row[k] == r) for r in range(7)] == [len(r) for r in ROWS]

    order = list(range(1, 31))
    sols = []

    def search(i, m, used):
        if i == len(order):
            sols.append(dict(m))
            return
        t = order[i]
        for k in range(30):
            if k in used or face_row[k] != tile_row[t]:
                continue
            if all((m[n] in faces[k]['adj']) for n in NEIGHBOURS[t] if n in m):
                m[t] = k
                used.add(k)
                search(i + 1, m, used)
                used.discard(k)
                del m[t]

    search(0, {}, set())
    return sols


def tile_geometry(rotate_steps=0, mirror=False):
    """{tile: (center[3], corners[4,3])}. rotate_steps (0..2) / mirror pick one of the 6
    symmetric solutions - which one is physically right has to be checked on the lamp."""
    faces = build_faces()
    sols = match_tiles(faces)
    # sort solutions in a stable way: by azimuth of tile 1, then handedness
    def key(s):
        c1, c4 = faces[s[1]]['center'], faces[s[4]]['center']
        az1 = np.degrees(np.arctan2(c1[1], c1[0])) % 360
        hand = np.cross(c1, c4)[2] > 0
        return (hand, round(az1, 3))
    sols.sort(key=key)
    s = sols[(3 if mirror else 0) + rotate_steps % 3] if len(sols) == 6 else sols[0]
    return {t: (faces[k]['center'], faces[k]['corners']) for t, k in s.items()}


if __name__ == '__main__':
    faces = build_faces()
    assert all(len(f['adj']) == 4 for f in faces)
    # the firmware table must be a symmetric 4-regular graph and the chain a path along it
    for t, ns in NEIGHBOURS.items():
        for n in ns:
            assert t in NEIGHBOURS[n], (t, n)
    print('chain 1..30 is a path over neighbouring faces:',
          all(t + 1 in NEIGHBOURS[t] for t in range(1, 30)))
    sols = match_tiles(faces)
    print('mappings respecting neighbours + rings:', len(sols))
