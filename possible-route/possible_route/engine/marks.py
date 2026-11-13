# Lateral marks from the chart: red can on a pole (192,0,0) and green square
# on a pole (0,242,121). The mark's position is the foot of the pole.
import os, sys, json, numpy as np, tiles
from scipy import ndimage
tiles.open_chart(CFG["chart"])
FOLDER = CFG["cache"]
d = np.load(os.path.join(FOLDER, "grid.npz"))
G, px0, py0, Z, B = d["grid"], int(d["px0"]), int(d["py0"]), int(d["zoom"]), int(d["cell"])
H, W = G.shape[0] * B, G.shape[1] * B
COL = {"red": (192, 0, 0), "green": (0, 242, 121)}
marks, odd = [], []
STEP = 2048
for y in range(0, H, STEP):
    for x in range(0, W, STEP):
        la0, lo0 = tiles.pix_ll(px0 + x, py0 + min(y + STEP + 64, H), Z)
        la1, lo1 = tiles.pix_ll(px0 + min(x + STEP + 64, W), py0 + y, Z)
        img, ox, oy = tiles.mosaic(la0 + 1e-6, lo0 + 1e-6, la1 - 1e-6, lo1 - 1e-6, Z)
        a = np.asarray(img).astype(int)
        for kind, c in COL.items():
            m = (abs(a[..., 0] - c[0]) <= 8) & (abs(a[..., 1] - c[1]) <= 8) & (abs(a[..., 2] - c[2]) <= 8)
            lab, n = ndimage.label(m, structure=np.ones((3, 3)))
            for i, s in enumerate(ndimage.find_objects(lab), 1):
                gx, gy = ox + s[1].start, oy + s[0].start
                if not (px0 + x <= gx < px0 + x + STEP and py0 + y <= gy < py0 + y + STEP): continue  # owned by this chunk
                comp = lab[s] == i
                if comp.sum() < 20: continue
                # walk down the symbol's centre line through pole, body and the
                # black foot dot until we reach chart background
                cx = int(round(s[1].start + np.where(comp[-1])[0].mean()))
                yy = s[0].stop; last = yy - 1; run = 0
                while yy < a.shape[0] and yy - s[0].stop < 45:
                    px = a[yy, max(0, cx - 1):cx + 2]
                    r_, g_, b_ = px[:, 0], px[:, 1], px[:, 2]
                    same = ((r_ > 100) & (g_ < 60) & (b_ < 60)) if kind == "red" else ((g_ > 120) & (r_ < 60) & (b_ < 160))
                    sym = same | (px.sum(1) < 150)
                    if not sym.any(): break
                    blk = (px.sum(1) < 150).any() and not same.any()
                    if blk: run += 1
                    elif run >= 2 and yy - s[0].stop > 4: break   # just left the foot dot
                    else: run = 0
                    last = yy; yy += 1
                fx, fy = ox + cx + 0.5, oy + last + 0.5
                la, lo = tiles.pix_ll(fx, fy, Z)
                marks.append({"kind": kind, "h": int(comp.shape[0]), "w": int(comp.shape[1]),
                              "px": fx, "py": fy, "lat": la, "lon": lo, "pole": int(last - s[0].stop + 1)})
# one mark per foot
marks.sort(key=lambda m: -m["pole"])
keep = []
for m in marks:
    if all(abs(m["px"] - k["px"]) > 4 or abs(m["py"] - k["py"]) > 6 for k in keep): keep.append(m)
odd = [m for m in keep if m["pole"] < 2]
marks = [m for m in keep if m["pole"] >= 2]
json.dump({"marks": marks, "odd": odd}, open(os.path.join(FOLDER, "marks.json"), "w"))
print(len(marks), "marks:", sum(m["kind"] == "red" for m in marks), "red,", sum(m["kind"] == "green" for m in marks), "green;", len(odd), "odd shapes")
from collections import Counter
print(Counter((o["kind"], o["h"], o["w"]) for o in odd).most_common(12)); print(Counter(m["pole"]//5*5 for m in marks))
