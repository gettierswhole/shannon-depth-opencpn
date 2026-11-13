# Read soundings from the cut-out characters: match digits against templates
# taken from the chart itself, join neighbours into figures like 6'2", and
# keep only figures that parse cleanly. Anything doubtful is set aside.
import os, sys, json, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__)); import tiles
d = np.load(os.path.join(CFG["cache"], "glyphs.npz")); S, cnt, ids, R = d["shapes"], d["shape_counts"], d["shape_ids"], d["rows"]
# shapes by how often they occur: the digits are the commonest after the quote stroke
order = np.argsort(-cnt, kind="quicksort")
def bm(k):
    b = S[ids[order[k]]]; ys, xs = np.nonzero(b); return b[:ys.max() + 1, :xs.max() + 1]
# Labels from the sheet: sheet index -> digit
LAB = {1: "1", 2: "2", 4: "3", 5: "4", 6: "5", 7: "6", 8: "7", 9: "0", 10: "8", 11: "9"}
T = {v: bm(k) for k, v in LAB.items()}
TH = int(np.median([t.shape[0] for t in T.values()]))
print("digit height", TH, {k: t.shape for k, t in T.items()})
def fit(b, shape):
    H, W = shape; out = np.zeros(shape, bool)
    out[:min(H, b.shape[0]), :min(W, b.shape[1])] = b[:H, :W]; return out
def classify(b):
    h, w = b.shape
    if h <= TH * 0.5 and w <= 3: return "q"            # a stroke of ' or "
    if abs(h - TH) > 1: return None
    scores = {}
    for k, t in T.items():
        if abs(t.shape[1] - w) > 2: continue
        W = max(w, t.shape[1]) + 2; Hh = TH + 3
        A = np.zeros((Hh, W), bool); A[1:1 + h, 1:1 + w] = b
        best_k = 0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                Bt = np.zeros((Hh, W), bool)
                y0, x0 = 1 + dy, 1 + dx
                th, tw = t.shape
                Bt[max(0, y0):y0 + th, max(0, x0):x0 + tw] = t[max(0, -y0):, max(0, -x0):][:Hh - max(0, y0), :W - max(0, x0)]
                best_k = max(best_k, (A & Bt).sum() / max(1, (A | Bt).sum()))
        scores[k] = best_k
    if not scores: return None
    ranked = sorted(scores.items(), key=lambda kv: -kv[1])
    best = ranked[0][1]; second = ranked[1][1] if len(ranked) > 1 else 0
    return ranked[0][0] if best >= 0.62 and best - second >= 0.06 else None
shape_lab = {}
for sid in np.unique(R[:, 4]):
    if sid < 0: shape_lab[sid] = None; continue
    b = S[sid]; ys, xs = np.nonzero(b); shape_lab[sid] = classify(b[:ys.max() + 1, :xs.max() + 1])
# Join characters into figures: same text line, small gaps.
rows = sorted(R.tolist(), key=lambda r: (r[1] // 4, r[0]))
items = [(x, y, w, h, shape_lab.get(s)) for x, y, w, h, s in R.tolist()]
items.sort(key=lambda t: (t[0]))
from collections import defaultdict
used = [False] * len(items)
byx = items
figs = []
import bisect
xs = [t[0] for t in byx]
for i, (x, y, w, h, l) in enumerate(byx):
    if used[i] or l is None or l == "q": continue
    # a figure that starts just right of an unreadable digit-high character is doubtful
    left_bad = False
    for j in range(max(0, bisect.bisect_left(xs, x - 20)), i):
        jx, jy, jw, jh, jl = byx[j]
        if not used[j] and abs(jy - y) <= 2 and 0 <= x - (jx + jw) <= 7 and abs(jh - TH) <= 1 and jl is None:
            left_bad = True
    # start of a figure: a digit with no digit immediately to its left on the line
    chain = [i]; cur = i
    while True:
        cx, cy, cw, ch, _ = byx[cur]; right = cx + cw
        j0 = bisect.bisect_left(xs, right - 1); nxt = None
        for j in range(j0, len(byx)):
            jx, jy, jw, jh, jl = byx[j]
            if used[j] or j in chain: continue
            if jx - right > 7: break
            top_ok = abs(jy - byx[i][1]) <= 2
            digit_high = abs(jh - TH) <= 1
            if top_ok and jx >= right - 1 and (jl is not None or digit_high): nxt = j; break
        if nxt is None: break
        chain.append(nxt); cur = nxt
    s = "".join(byx[k][4] or "?" for k in chain).replace("qq", '"').replace("q", "'")
    if left_bad: s = "?" + s
    m = re.fullmatch(r"(\d{1,3})'(?:(\d{1,2})\")?", s)
    # The chart prints inches on every sounding under 10 ft and never on deeper
    # ones; a figure that breaks that has lost part of itself.
    if m and ((int(m.group(1)) < 10) != (m.group(2) is not None)): m = None
    for k in chain: used[k] = True
    x0 = byx[chain[0]][0]; x1 = byx[chain[-1]][0] + byx[chain[-1]][2]
    y0 = min(byx[k][1] for k in chain); y1 = max(byx[k][1] + byx[k][3] for k in chain)
    # Anything else drawn touching the figure (a mark, its pole, another
    # character) means part of it may be hidden: set it aside.
    crowded = False
    for j in range(bisect.bisect_left(xs, x0 - 30), bisect.bisect_right(xs, x1 + 8)):
        if j in chain: continue
        jx, jy, jw, jh, jl = byx[j]
        if jy + jh < y0 - 1 or jy > y1 + 1: continue
        if jx + jw >= x0 - 7 and jx <= x1 + 7: crowded = True; break
    if crowded: s = "?" + s
    figs.append({"text": s, "ok": bool(m) and (m.group(2) is None or int(m.group(2)) < 12),
                 "feet": int(m.group(1)) if m else None, "inches": int(m.group(2) or 0) if m else None,
                 "px": (x0 + x1) / 2, "py": (y0 + y1) / 2, "box": [int(x0), int(y0), int(x1), int(y1)]})
ok = [f for f in figs if f["ok"]]
for f in ok:
    f["lat"], f["lon"] = tiles.pix_ll(f["px"], f["py"], 16)
    f["depth_ft"] = f["feet"] + f["inches"] / 12
json.dump({"soundings": ok, "doubtful": [f for f in figs if not f["ok"]]},
          open(os.path.join(CFG["cache"], "soundings.json"), "w"))
print(len(figs), "figures;", len(ok), "read cleanly;", len(figs) - len(ok), "set aside")
print("under 6'6\":", sum(1 for f in ok if f["depth_ft"] < 6.5))
from collections import Counter
print(Counter(f["text"] for f in figs if not f["ok"]).most_common(15))
