# Locks and weirs, read from the chart at pixel level.
# Weir and lock walls are drawn as grey lines and fills: those become walls.
# The way through a lock is found by following open water from above the lock
# to below it without crossing a grey line: that path, through the chamber,
# becomes a corridor the router may use even though it is narrower than the
# usual clearance.
import os, json, math, numpy as np, tiles
from scipy import ndimage
from skimage.graph import MCP_Geometric
tiles.open_chart(CFG["chart"])
F = CFG["cache"] + os.sep
d = np.load(F + "grid.npz"); G, px0, py0, Z, B = d["grid"], int(d["px0"]), int(d["py0"]), int(d["zoom"]), int(d["cell"])
DAT = json.load(open(CFG["datums"]))
zones = json.load(open(CFG["boundaries"]))["zones"]
polys = {st["id"]: st["boundary"] for st in DAT["stretches"]}
out = []
for z in zones:
    if "Lock" not in z["name"]: continue
    x, y = tiles.tile_xy(z["lat"], z["lon"], Z)
    gx, gy = x * 256, y * 256
    if not (px0 <= gx < px0 + G.shape[1] * B and py0 <= gy < py0 + G.shape[0] * B): continue
    R = 400   # m: look at the lock and its approaches
    dl = R / 111000; dlo = R / (111000 * math.cos(math.radians(z["lat"])))
    img, ox, oy = tiles.mosaic(z["lat"] - dl, z["lon"] - dlo, z["lat"] + dl, z["lon"] + dlo, Z)
    a = np.asarray(img).astype(int); r_, g_, b_ = a[..., 0], a[..., 1], a[..., 2]
    grey = (abs(r_ - g_) <= 8) & (abs(g_ - b_) <= 8) & (r_ >= 90) & (r_ <= 200)
    land = (r_ > 180) & (g_ > 140) & (b_ < 60) | ((g_ > 200) & (r_ < 60) & (b_ < 60)) | ((r_ == 128) & (g_ == 64))
    blue = (b_ > 200) & (r_ < 200)
    white = (r_ >= 240) & (g_ >= 240) & (b_ >= 240)
    # lock walls are also drawn as long black lines; text is compact
    blk = (r_ + g_ + b_) < 60
    lab, n = ndimage.label(blk, structure=np.ones((3, 3)))
    lines = np.zeros_like(blk)
    cy0, cx0 = int(gy - oy), int(gx - ox)
    for i, sl in enumerate(ndimage.find_objects(lab), 1):
        h, w = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        comp = lab[sl] == i
        if max(h, w) >= 18 and comp.sum() < 0.35 * h * w and math.dist(((sl[0].start + sl[0].stop) / 2, (sl[1].start + sl[1].stop) / 2), (cy0, cx0)) < 150:
            lines[sl] |= comp
    grey = grey | lines
    water = (blue | white) & ~grey
    # which side each water pixel is on, from the stretch outlines
    H, W = a.shape[:2]
    lat = np.array([tiles.pix_ll(ox, oy + i, Z)[0] for i in range(H)])
    lon = np.array([tiles.pix_ll(ox + j, oy, Z)[1] for j in range(W)])
    P = np.column_stack([np.repeat(lat, W), np.tile(lon, H)])
    side = {k: tiles.contains(p, P).reshape(H, W) & water for k, p in polys.items()}
    sides = [k for k, m in side.items() if m.sum() > 200]
    info = {"name": z["name"], "lat": z["lat"], "lon": z["lon"], "sides": sides, "corridor": [], "walls": []}
    if len(sides) == 2:
        cost = np.where(water, 1.0, np.where(grey | land, np.inf, 25.0))   # text and outlines can be crossed, at a price
        A, Bm = side[sides[0]], side[sides[1]]
        # start from the upstream water nearest the lock, end at downstream water
        ia = np.argwhere(A); ib = np.argwhere(Bm)
        cy, cx = int(gy - oy), int(gx - ox)
        s = tuple(ia[np.argmin(((ia - (cy, cx)) ** 2).sum(1))]); e = tuple(ib[np.argmin(((ib - (cy, cx)) ** 2).sum(1))])
        m = MCP_Geometric(cost, fully_connected=True); cum, _ = m.find_costs([s], [e])
        if np.isfinite(cum[e]):
            path = m.traceback(e)
            # keep only the narrow part (the chamber and its gates); the
            # approaches either side are ordinary water for the router
            dt = ndimage.distance_transform_edt(water | ~(grey | land))
            narrow = [dt[p] < 7 for p in path]
            ci = int(np.argmin([math.dist(p, (cy, cx)) for p in path]))
            lo_, hi_ = ci, ci
            while lo_ > 0 and narrow[lo_ - 1]: lo_ -= 1
            while hi_ < len(path) - 1 and narrow[hi_ + 1]: hi_ += 1
            path = path[max(0, lo_ - 2):hi_ + 3]
            info["corridor"] = sorted({(int((oy + p[0] - py0) // B), int((ox + p[1] - px0) // B)) for p in path})
            info["pixels"] = len(path)
            info["crosses_text"] = int(sum(not water[p] for p in path))
    # walls: grid cells holding a run of grey (weir crest, lock walls)
    gc = {}
    for (i, j) in np.argwhere(grey & ~ndimage.binary_opening(grey, np.ones((9, 9)))):   # thin grey only, not grey fill areas
        k = (int((oy + i - py0) // B), int((ox + j - px0) // B)); gc[k] = gc.get(k, 0) + 1
    corr = set(map(tuple, info["corridor"]))
    # only near this lock and its weir(s): bridges further off are not walls
    centres = [(z2["lat"], z2["lon"], z2["r"]) for z2 in zones
               if "Weir" in z2["name"] and math.dist((z2["lat"], z2["lon"]), (z["lat"], z["lon"])) < 0.01] + [(z["lat"], z["lon"], 120)]
    def near(k):
        la, lo = tiles.pix_ll(px0 + (k[1] + 0.5) * B, py0 + (k[0] + 0.5) * B, Z)
        return any(math.hypot((la - a) * 111000, (lo - b) * 111000 * math.cos(math.radians(a))) <= r for a, b, r in centres)
    W_ = {k for k, n in gc.items() if n >= 1 and k not in corr and 0 <= k[0] < G.shape[0] and 0 <= k[1] < G.shape[1] and G[k] != 3 and near(k)}
    info["walls"] = sorted(W_)
    out.append(info)
    print(z["name"], "sides", sides, "corridor cells", len(info["corridor"]), "through", info.get("pixels"), "px, crossing", info.get("crosses_text"), "px of drawing;", len(info["walls"]), "wall cells")
json.dump(out, open(F + "locks.json", "w"))
