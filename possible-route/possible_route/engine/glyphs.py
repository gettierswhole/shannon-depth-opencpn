# Cut every dark character out of the chart's zoom-16 tiles over an area and
# group identical shapes. Output: glyphs.npz with each character's position,
# box and shape key, plus the shapes themselves.
import os, sys, io, math, numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(__file__)); import tiles
Z = 16
tiles.open_chart(CFG["chart"])
lat0, lon0, lat1, lon1 = CFG["bbox"]
x0, y0 = tiles.tile_xy(lat1, lon0, Z); x1, y1 = tiles.tile_xy(lat0, lon1, Z)
tx0, ty0, tx1, ty1 = int(x0), int(y0), int(x1), int(y1)
n = 2 ** Z
have = set((c, n - 1 - r) for c, r in tiles.CHART.execute(
    "select tile_column, tile_row from tiles where zoom_level=? and tile_column between ? and ? and tile_row between ? and ?",
    (Z, tx0, tx1, n - 1 - ty1, n - 1 - ty0)))
cache = {}
def get(tx, ty):
    if (tx, ty) not in have: return None
    if (tx, ty) not in cache:
        if len(cache) > 400: cache.clear()
        r = tiles.CHART.execute("select tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?", (Z, tx, n - 1 - ty)).fetchone()
        cache[(tx, ty)] = np.asarray(Image.open(io.BytesIO(r[0])).convert("RGB"))
    return cache[(tx, ty)]
M = 24
rows = []; shapes = {}
for ty in range(ty0, ty1 + 1):
    for tx in range(tx0, tx1 + 1):
        if (tx, ty) not in have: continue
        win = np.full((256 + 2 * M, 256 + 2 * M, 3), 248, np.uint8)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                t = get(tx + dx, ty + dy)
                if t is None: continue
                ys, xs = slice(max(0, M - 256 + 256 * (dy + 1) - M + M) if False else None, None), None
                # paste the overlapping part
                Y0, X0 = M + dy * 256, M + dx * 256
                sy0, sx0 = max(0, -Y0), max(0, -X0)
                ey, ex = min(256, 256 + 2 * M - Y0), min(256, 256 + 2 * M - X0)
                if ey > sy0 and ex > sx0:
                    win[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = t[sy0:ey, sx0:ex]
        a = win.astype(int)
        lum = (a[..., 0] * 3 + a[..., 1] * 6 + a[..., 2]) / 10
        dark = (lum < 110) & (np.abs(a[..., 0] - a[..., 2]) < 70)
        lab, k = ndimage.label(dark, structure=np.ones((3, 3)))
        if not k: continue
        for i, sl in enumerate(ndimage.find_objects(lab), 1):
            h, w = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
            cy, cx = (sl[0].start + sl[0].stop) / 2, (sl[1].start + sl[1].stop) / 2
            if not (M <= cy < M + 256 and M <= cx < M + 256): continue
            if h * w < 3: continue
            if h > 22 or w > 22:
                # too big to be a character (a line, a pole, a symbol merged with
                # a digit): kept only so that figures touching it are set aside
                gx, gy = tx * 256 + sl[1].start - M, ty * 256 + sl[0].start - M
                rows.append((gx, gy, w, h, -1)); continue
            bm = (lab[sl] == i)
            key = (h, w, np.packbits(bm).tobytes())
            if key not in shapes: shapes[key] = [len(shapes), 0, bm]
            shapes[key][1] += 1
            gx, gy = tx * 256 + sl[1].start - M, ty * 256 + sl[0].start - M
            rows.append((gx, gy, w, h, shapes[key][0]))
R = np.array(rows, np.int64)
ids = np.array([v[0] for v in shapes.values()]); cnt = np.array([v[1] for v in shapes.values()])
maxh = max(v[2].shape[0] for v in shapes.values()); maxw = max(v[2].shape[1] for v in shapes.values())
S = np.zeros((len(shapes), maxh, maxw), bool)
for v in shapes.values(): S[v[0], :v[2].shape[0], :v[2].shape[1]] = v[2]
np.savez_compressed(os.path.join(CFG["cache"], "glyphs.npz"), rows=R, shape_ids=ids, shape_counts=cnt, shapes=S)
print(len(R), "characters,", len(shapes), "distinct shapes; top counts", sorted(cnt)[-12:])
