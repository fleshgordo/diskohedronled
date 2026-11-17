"""Draw which board (tile = position in the LED chain) sits on which face.
    python3 tools/sphere/layout.py  -> docs/tile_layout.png
"""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from geometry import tile_geometry, ROWS, NEIGHBOURS

OUT = Path(__file__).resolve().parents[2] / 'docs' / 'tile_layout.png'
ROW_COL = ['#ffb347', '#ffd27f', '#9bd3ae', '#7fb7e6', '#b39ddb', '#f48fb1', '#e57373']


def draw_3d(ax, geo, elev, azim, title):
    row_of = {t: r for r, row in enumerate(ROWS) for t in row}
    polys = [geo[t][1] for t in range(1, 31)]
    cols = [ROW_COL[row_of[t]] for t in range(1, 31)]
    ax.add_collection3d(Poly3DCollection(polys, facecolors=cols, edgecolors='#333', linewidths=0.8, alpha=1))
    e, a = np.radians(elev), np.radians(azim)
    view = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)])
    for t in range(1, 31):
        c = geo[t][0]
        if c @ view > 0.15:
            p = c * 1.62
            ax.text(*p, str(t), ha='center', va='center', fontsize=11, weight='bold', zorder=10)
    # chain path on the visible side
    for t in range(1, 30):
        a1, a2 = geo[t][0], geo[t + 1][0]
        if a1 @ view > 0.15 and a2 @ view > 0.15:
            ax.plot(*np.array([a1, a2]).T * 1.6, color='#c00', lw=1.2, alpha=0.7)
    ax.view_init(elev, azim)
    lim = 1.25
    ax.set_xlim(-lim, lim); ax.set_ylim(-lim, lim); ax.set_zlim(-lim, lim)
    ax.set_box_aspect((1, 1, 1))
    ax.set_axis_off()
    ax.set_title(title, fontsize=11)


def draw_map(ax, geo):
    for t in range(1, 31):
        c = geo[t][0]
        lon = np.degrees(np.arctan2(c[1], c[0]))
        lat = np.degrees(np.arcsin(np.clip(c[2], -1, 1)))
        ax.scatter(lon, lat, s=420, color=ROW_COL[[r for r, row in enumerate(ROWS) if t in row][0]],
                   edgecolor='#333', zorder=3)
        ax.text(lon, lat, str(t), ha='center', va='center', fontsize=9, weight='bold', zorder=4)
    for t in range(1, 30):
        c1, c2 = geo[t][0], geo[t + 1][0]
        l1, l2 = np.degrees(np.arctan2(c1[1], c1[0])), np.degrees(np.arctan2(c2[1], c2[0]))
        b1, b2 = np.degrees(np.arcsin(c1[2])), np.degrees(np.arcsin(c2[2]))
        if abs(l1 - l2) < 180:
            ax.annotate('', xy=(l2, b2), xytext=(l1, b1),
                        arrowprops=dict(arrowstyle='-|>', color='#c00', lw=1, shrinkA=10, shrinkB=10), zorder=2)
    ax.set_xlim(-185, 185); ax.set_ylim(-90, 90)
    ax.set_xlabel('longitude (°)  –  0° = "front", see note'); ax.set_ylabel('latitude (°)')
    ax.set_title('Flat map (like an equirectangular video frame). Red arrows = LED chain order', fontsize=11)
    ax.grid(alpha=0.3)


def main():
    geo = tile_geometry()
    fig = plt.figure(figsize=(16, 15))
    fig.suptitle('Diskohedron LED – which board sits on which face (number = position in the LED chain)',
                 fontsize=15, y=0.985)
    views = [(90, -90, 'From above (rings 1–3 on top)'), (8, 0, 'Side, looking from 0°'),
             (8, 120, 'Side, looking from 120°'), (8, 240, 'Side, looking from 240°'),
             (-90, -90, 'From below')]
    for i, (e, a, t) in enumerate(views):
        draw_3d(fig.add_subplot(3, 3, i + 1, projection='3d'), geo, e, a, t)
    leg = fig.add_subplot(3, 3, 6)
    leg.axis('off')
    for r, row in enumerate(ROWS):
        leg.add_patch(plt.Rectangle((0.02, 0.88 - r * 0.12), 0.07, 0.08, color=ROW_COL[r], ec='#333'))
        leg.text(0.12, 0.92 - r * 0.12, f'ring {r + 1}: tiles {", ".join(map(str, row))}', va='center', fontsize=10.5)
    leg.text(0.02, -0.05, 'Up/down follow the old firmware (ring 1 = "top").\n'
                          'Which side is "front", and mirror/not, must be checked\n'
                          'on the lamp (light tile 1 and tile 4 and compare).', fontsize=10, va='top')
    draw_map(fig.add_subplot(3, 1, 3), geo)
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    OUT.parent.mkdir(exist_ok=True)
    fig.savefig(OUT, dpi=110)
    print('wrote', OUT)


if __name__ == '__main__':
    main()
