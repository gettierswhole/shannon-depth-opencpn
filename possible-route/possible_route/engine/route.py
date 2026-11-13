# Possible route, first look: shortest path through open water on the chart,
# keeping out of the shaded shallows, land and marks with a safety buffer.
import sys, os, json, math, numpy as np
from scipy import ndimage
from skimage.graph import MCP_Geometric
sys.path.insert(0, os.path.dirname(__file__)); import tiles
FOLDER = CFG["cache"]; OUT = CFG["out"]
d = np.load(os.path.join(FOLDER, "grid.npz"))
G, px0, py0, Z, B = d["grid"], int(d["px0"]), int(d["py0"]), int(d["zoom"]), int(d["cell"])
# Mark symbols are drawn standing up from the mark, so their pixels cover water
# that is really open. Give those cells the class of the nearest other cell;
# the marks themselves come back below at the foot of each pole.
_is4 = G == 4
if _is4.any():
    _, (_ir, _ic) = ndimage.distance_transform_edt(_is4, return_indices=True)
    G = G[_ir, _ic]
lat_mid = 53.58
cell_m = 156543.03 * math.cos(math.radians(lat_mid)) / 2 ** Z * B
MIN_CLEAR_M = float(CFG.get("min_clear_m", 10))   # hard: about one cell clear of a hazard
PREFER_M = 40                                           # soft: a mild preference for mid-channel
# Depth rule: the charted depth needed is draught + margin - today's correction
# at the shallow end of the band.
FT = 0.3048
DRAUGHT_FT = float(CFG["draught_ft"])
MARGIN_FT = float(CFG["margin_ft"])
# Each stretch reads its own gauge: the water level there, less the stretch's
# chart datum, is today's correction; the band is how unsure that is. A lock
# is just the step between two stretches, each with its own level.
DAT = json.load(open(CFG["datums"]))
LEV = CFG["levels"]
# cell centres: latitude depends on row only, longitude on column only
_lat_r = np.array([tiles.pix_ll(px0, py0 + (r + 0.5) * B, Z)[0] for r in range(G.shape[0])])
_lon_c = np.array([tiles.pix_ll(px0 + (c + 0.5) * B, py0, Z)[1] for c in range(G.shape[1])])
_pts = np.column_stack([np.repeat(_lat_r, G.shape[1]), np.tile(_lon_c, G.shape[0])])
STRETCH = np.full(G.shape, -1, int)
CORR = {}
for k, st in enumerate(DAT["stretches"]):
    g = LEV.get(st["gauge"]["doc"])
    if not g: continue
    inside = tiles.contains(st["boundary"], _pts).reshape(G.shape)
    STRETCH[inside & (STRETCH < 0)] = k
    CORR[k] = (g["v"] - st["datum"]["level"], st["band"])
    print(f'{st["name"]}: {st["gauge"]["name"]} {g["v"]:.3f} m, {(g["v"] - st["datum"]["level"]) / FT * 12:+.0f} in on chart datum')
# The stretch outlines stop short of each other around a lock (the old "no
# figure near locks" buffer). For routing, water in that gap belongs to the
# stretch below the lock: the lower of the two levels, which is the water you
# actually have below the gates.
for z in json.load(open(CFG["boundaries"]))["zones"]:
    x, y = tiles.tile_xy(z["lat"], z["lon"], Z)
    zc, zr = int((x * 256 - px0) / B), int((y * 256 - py0) / B)
    R_ = int((z["r"] + 300) / cell_m)
    ya, yb, xa, xb = max(0, zr - R_), min(G.shape[0], zr + R_ + 1), max(0, zc - R_), min(G.shape[1], zc + R_ + 1)
    if ya >= yb or xa >= xb: continue
    win = STRETCH[ya:yb, xa:xb]
    near = [k for k in set(win[win >= 0].ravel().tolist()) if k in CORR]
    if not near: continue
    low = min(near, key=lambda k: LEV[DAT["stretches"][k]["gauge"]["doc"]]["v"])
    win[(win < 0) & (G[ya:yb, xa:xb] != 3)] = low
NEED = np.full(G.shape, np.inf)
for k, (corr, band) in CORR.items():
    NEED[STRETCH == k] = DRAUGHT_FT + MARGIN_FT - (corr - band) / FT
need_ft = NEED
nogauge = (STRETCH < 0) & (G != 3)
LIGHT_MIN_FT, MID_MIN_FT = 4.5, 2.5          # shallowest typical sounding in each shading band
passable = (G == 0) | ((G == 1) & (need_ft <= LIGHT_MIN_FT)) | ((G == 6) & (need_ft <= MID_MIN_FT))
# Darker blue covers roughly 2.5 to 6 ft. Where it is surveyed, the soundings
# say how deep it really is: a darker-blue cell opens when every readable
# sounding within EVIDENCE_M is at least what we need (and there is at least
# one). Unreadable figures neither open nor block; any near the route goes
# on the hand-check list.
EVIDENCE_M = float(CFG.get("evidence_m", 150))
from scipy.spatial import cKDTree
_S = json.load(open(os.path.join(FOLDER, "soundings.json")))
import re
# A part-read figure such as 3'?" still gives a floor: an inch mark means the
# feet are under 10 and fully shown, so the depth is at least that many feet.
_floor = []
# A stray mark before the figure (?3'7') is noise from a neighbouring symbol:
# with an inch figure present the feet are under 10, so no digit was lost, and
# a closing ' for " is a misread quote.
_PART = re.compile(r"^\?*(\d)'(?:(\d{1,2})[\"'])?")
for x in _S["doubtful"]:
    m = _PART.match(x["text"])
    if m:
        inch = int(m.group(2)) if m.group(2) and int(m.group(2)) <= 11 else 0
        exact = bool(m.group(2)) and inch == int(m.group(2))
        _floor.append({**x, "depth_ft": int(m.group(1)) + inch / 12, "floor": not exact, "reread": exact})
_S["doubtful"] = [x for x in _S["doubtful"] if not _PART.match(x["text"])]
SND_EXTRA = _floor
_good = np.array([[(x["py"] - py0) / B, (x["px"] - px0) / B, x["depth_ft"]] for x in _S["soundings"] + _floor])
_bad = np.array([[(x["py"] - py0) / B, (x["px"] - px0) / B] for x in _S["doubtful"]]) if _S["doubtful"] else np.zeros((0, 2))
_mid = np.argwhere(((G == 6) | (G == 1)) & ~passable)   # either blue band, where soundings vouch for it
if len(_mid):
    Rc = EVIDENCE_M / cell_m
    tg = cKDTree(_good[:, :2]); near = tg.query_ball_point(_mid + 0.5, Rc)
    dg, _ = tg.query(_mid + 0.5)
    db = cKDTree(_bad).query(_mid + 0.5)[0] if len(_bad) else np.full(len(_mid), np.inf)
    opened = 0
    for (r, c), idx, g1, b1 in zip(_mid, near, dg, db):
        # judge by the nearest figures, not every figure in range: the nearest
        # one and any others no more than half as far again
        if idx:
            dd_ = np.hypot(_good[idx, 0] - (r + 0.5), _good[idx, 1] - (c + 0.5))
            idx = [i for i, d_ in zip(idx, dd_) if d_ <= 1.5 * dd_.min() + 1]
        if idx and _good[idx, 2].min() >= NEED[r, c]:
            passable[r, c] = True; opened += 1
    print(f"shaded water opened by soundings: {opened} of {len(_mid)} cells")
# Bridges are drawn as lines across the water and read as land: let the
# route through land that is no more than two cells (about 20 m) thick with
# water on both sides. Each such crossing goes on the hand-check list.
from scipy import ndimage as _nd
bridged = np.zeros_like(passable)
for st in (np.ones((3, 1), bool), np.ones((1, 3), bool), np.ones((4, 1), bool), np.ones((1, 4), bool)):
    bridged |= _nd.binary_closing(passable, structure=st) & ~passable & (G == 3)
# ...but never the edge of a solid piece of land (an island's tip or a
# shoreline): only thin lines whose cells are well away from any thick land.
_land = G == 3
_core = ndimage.binary_erosion(_land, structure=np.ones((3, 3)))
_near_core = ndimage.binary_dilation(_core, structure=np.ones((3, 3)))
bridged_raw = bridged.copy()
bridged &= ~_near_core
passable = passable | bridged
blocked = ~passable
# every sounding shallower than needed is a small hazard of its own
SND = json.load(open(os.path.join(FOLDER, "soundings.json")))["soundings"]
shoal = 0
for snd in SND + SND_EXTRA:
    cx, cy = int((snd["px"] - px0) / B), int((snd["py"] - py0) / B)
    if not (0 <= cy < G.shape[0] and 0 <= cx < G.shape[1]) or snd["depth_ft"] >= NEED[cy, cx]: continue
    if True:
        blocked[max(0, cy - 1):cy + 2, max(0, cx - 1):cx + 2] = True; shoal += 1
print(f"need {np.nanmin(NEED[np.isfinite(NEED)]):.2f}-{np.nanmax(NEED[np.isfinite(NEED)]):.2f} ft charted; {shoal} soundings shallower than that; {int(nogauge.sum())} water cells with no gauge")
# Locks are not walls. A lock is passable when the water over each cill, on
# its own side's gauge, gives the draught plus margin. Where a gauge sits right
# at the lock (Athlone has one above and one below the weir) that reading is
# used; otherwise the stretch's own gauge.
LOCK_GAUGES = {"Athlone": {"upper": "athloneus", "lower": "athlone"}}
LOCKS = []
_cills = [(c, st) for st in DAT["stretches"] for c in st.get("cills", [])]
for z in json.load(open(CFG["boundaries"]))["zones"]:
    x, y = tiles.tile_xy(z["lat"], z["lon"], Z)
    cx, cy = int((x * 256 - px0) / B), int((y * 256 - py0) / B)
    if not (0 <= cy < G.shape[0] and 0 <= cx < G.shape[1]) or "Lock" not in z["name"]: continue
    place = z["name"].split(" Lock")[0].split(",")[-1].strip()
    info = {"name": z["name"], "lat": z["lat"], "lon": z["lon"], "rc": (cy, cx), "cills": []}
    for c, st in _cills:
        if not place or place.split()[0] not in c["name"]: continue
        side = "upper" if "upper" in c["name"] else "lower"
        gdoc = LOCK_GAUGES.get(place, {}).get(side, st["gauge"]["doc"])
        lv = LEV.get(gdoc, {}).get("v")
        if lv is None: continue
        over = (lv - c["level"]) / FT
        info["cills"].append({"cill": c["name"], "gauge": gdoc, "level": lv, "over_ft": round(over, 2),
                              "ok": over >= DRAUGHT_FT + MARGIN_FT})
    info["ok"] = all(x["ok"] for x in info["cills"])
    if not info["ok"]:      # can't clear a cill today: the lock really is shut to this boat
        r = int(z["r"] / cell_m) + 1
        blocked[max(0, cy - r):cy + r + 1, max(0, cx - r):cx + r + 1] |= (G[max(0, cy - r):cy + r + 1, max(0, cx - r):cx + r + 1] != 3)
    LOCKS.append(info)
for L in LOCKS:
    print("lock:", L["name"], "passable" if L["ok"] else "SHUT (cill)", [(c["cill"], c["over_ft"]) for c in L["cills"]] or "no cill data")
# Marks: block the foot of each, and close the gap between each mark and the
# nearest hazard behind it (shore or shallows), so the route passes a mark on
# its open-water side. The colour rule is checked against the result below.
MK = json.load(open(os.path.join(FOLDER, "marks.json")))
MARKS = MK["marks"] + MK["odd"]
GATE_MAX_M = 250
_hz = blocked.copy()
_, (_hr, _hc) = ndimage.distance_transform_edt(~_hz, return_indices=True)
gates = 0; refused = []
for mk in MARKS:
    r, c = int((mk["py"] - py0) / B), int((mk["px"] - px0) / B)
    if not (0 <= r < G.shape[0] and 0 <= c < G.shape[1]): continue
    mk["rc"] = (r, c)
    blocked[r, c] = True
    hr, hc = _hr[r, c], _hc[r, c]
    if math.dist((r, c), (hr, hc)) * cell_m > GATE_MAX_M: continue
    n = int(max(abs(hr - r), abs(hc - c))) + 1
    rr = np.linspace(r, hr, n).round().astype(int); cc = np.linspace(c, hc, n).round().astype(int)
    if mk.get("pole", 2) < 2 or mk.get("gauge"): continue          # foot not found, or a height gauge: block it, but no gate
    # one of a red-green pair: the pair itself shows the channel, and a gate to
    # the nearest shore could run straight across it
    other = "green" if mk["kind"] == "red" else "red"
    if any(o["kind"] == other and math.hypot((o["py"] - mk["py"]) / B, (o["px"] - mk["px"]) / B) * cell_m <= 60 for o in MARKS): continue
    # a gate may close the passage inside a mark, never the channel itself:
    # refuse any gate that splits the open water around the mark
    W_ = 20
    ya, yb, xa, xb = max(0, r - W_), min(G.shape[0], r + W_ + 1), max(0, c - W_), min(G.shape[1], c + W_ + 1)
    before = ~blocked[ya:yb, xa:xb]
    after = before.copy()
    for dr, dc in ((0, 0), (0, 1), (1, 0)):
        rr2 = np.clip(rr + dr, 0, G.shape[0] - 1); cc2 = np.clip(cc + dc, 0, G.shape[1] - 1)
        inwin = (rr2 >= ya) & (rr2 < yb) & (cc2 >= xa) & (cc2 < xb)
        after[rr2[inwin] - ya, cc2[inwin] - xa] = False
    def _big(m):
        l, n = ndimage.label(m, structure=np.ones((3, 3)))
        return int((np.bincount(l.ravel())[1:] >= 6).sum()) if n else 0
    if _big(after) > _big(before):
        refused.append(mk); continue
    blocked[ya:yb, xa:xb] &= after | blocked[ya:yb, xa:xb]
    blocked[ya:yb, xa:xb] |= ~after
    gates += 1
print(f"{len(MARKS)} marks, {gates} gated to the shore or shallows behind them, {len(refused)} gates refused (would cut the channel)")
def to_cell(lat, lon):
    x, y = tiles.tile_xy(lat, lon, Z)
    return int((y * 256 - py0) / B), int((x * 256 - px0) / B)
def to_ll(r, c):
    return tiles.pix_ll(px0 + (c + 0.5) * B, py0 + (r + 0.5) * B, Z)
# A red and a green within PAIR_M of each other are a gate pair: the water
# between them is the channel even when it is narrower than our clearance.
PAIR_M = 60
gap = np.zeros_like(blocked)
_rm = [m for m in MARKS if "rc" in m and m["kind"] == "red"]
_gm = [m for m in MARKS if "rc" in m and m["kind"] == "green" and not m.get("gauge")]
pairs = 0
for rm in _rm:
    for gm in _gm:
        dd = math.dist(rm["rc"], gm["rc"])
        if dd * cell_m > PAIR_M or dd < 1: continue
        n = int(max(abs(rm["rc"][0] - gm["rc"][0]), abs(rm["rc"][1] - gm["rc"][1]))) + 1
        rr = np.linspace(rm["rc"][0], gm["rc"][0], n).round().astype(int)
        cc = np.linspace(rm["rc"][1], gm["rc"][1], n).round().astype(int)
        # the line between the pair, and a short passage through its middle at
        # right angles to it, so a route can enter and leave the gate
        (ra, ca), (rb, cb) = rm["rc"], gm["rc"]
        mr, mc = (ra + rb) / 2, (ca + cb) / 2
        pr, pc = -(cb - ca) / dd, (rb - ra) / dd
        cand = list(zip(rr, cc))
        for t in np.arange(-3, 3.01, 0.5):
            for off in (-0.5, 0, 0.5):
                cand.append((int(round(mr + pr * t + (rb - ra) / dd * off)), int(round(mc + pc * t + (cb - ca) / dd * off))))
        for r_, c_ in cand:
            if (r_, c_) not in (tuple(rm["rc"]), tuple(gm["rc"])) and 0 <= r_ < G.shape[0] and 0 <= c_ < G.shape[1] and (G[r_, c_] != 3 or bridged_raw[r_, c_]):   # a bridge's navigation arch is a gate pair too
                gap[r_, c_] = True
        pairs += 1
print(f"{pairs} red-green gate pairs")
def to_cell(lat, lon):
    x, y = tiles.tile_xy(lat, lon, Z)
    return int((y * 256 - py0) / B), int((x * 256 - px0) / B)
# Weirs are walls; the way through a lock chamber is a corridor (lockpass.py)
LK = json.load(open(os.path.join(FOLDER, "locks.json")))
for L in LK:
    for r_, c_ in L["walls"]: blocked[r_, c_] = True
    shut = any(x["name"] == L["name"] and not x["ok"] for x in LOCKS)
    if L["corridor"] and not shut:
        pair_gap = gap.copy()
        for r_, c_ in L["corridor"]: gap[r_, c_] = True
        # a lock's lead-in marks are usually right at the gates: join each end
        # of the chamber to a gate pair within 70 m
        cc_ = np.array(L["corridor"]); dmat = ((cc_[:, None, :] - cc_[None, :, :]) ** 2).sum(-1)
        i_, j_ = np.unravel_index(np.argmax(dmat), dmat.shape)
        pg = np.argwhere(pair_gap)
        for end in (cc_[i_], cc_[j_]):
            if not len(pg): break
            dd_ = np.sqrt(((pg - end) ** 2).sum(1)); k_ = int(np.argmin(dd_))
            if dd_[k_] * cell_m <= 70:
                n_ = int(max(abs(pg[k_] - end))) + 1
                for t in np.linspace(0, 1, n_ + 1):
                    gap[int(round(end[0] + (pg[k_][0] - end[0]) * t)), int(round(end[1] + (pg[k_][1] - end[1]) * t))] = True
    # Near a lock the only way from one level to the other is the chamber:
    # wherever water of the two stretches meets, except in the chamber, wall it.
    if L["corridor"]:
        lr, lc = to_cell(L["lat"], L["lon"]); R_ = int(400 / cell_m)
        _cm = np.zeros_like(blocked); _cm[tuple(np.array(L["corridor"]).T)] = True
        near_corr = ndimage.binary_dilation(_cm, iterations=2)
        gap |= near_corr & (G != 3) & ~blocked     # the water at each end of the chamber leads into it
        ya, yb, xa, xb = max(1, lr - R_), min(G.shape[0] - 1, lr + R_), max(1, lc - R_), min(G.shape[1] - 1, lc + R_)
        for r_ in range(ya, yb):
            for c_ in range(xa, xb):
                k = STRETCH[r_, c_]
                if k < 0 or G[r_, c_] == 3 or gap[r_, c_] or near_corr[r_, c_]: continue
                nb = STRETCH[r_ - 1:r_ + 2, c_ - 1:c_ + 2]
                if ((nb >= 0) & (nb != k)).any(): blocked[r_, c_] = True
    print("lock:", L["name"], f'{len(L["walls"])} wall cells (weir and lock walls);',
          "way through the chamber found" if L["corridor"] else "no way through (only one stretch has a gauge)",
          "- but SHUT today on the cill" if shut else "")
def prepare():
    global dist, ok, cost, main
    blocked[gap] = False
    dist = ndimage.distance_transform_edt(~blocked) * cell_m
    ok = (dist >= MIN_CLEAR_M) | gap
    cost = np.where(ok, 1.0 + np.clip((PREFER_M - dist) / PREFER_M, 0, 1) * 1.0 + (G != 0) * 0.5, np.inf)
    lab, _n = ndimage.label(ok, structure=np.ones((3, 3)))
    _sizes = np.bincount(lab.ravel()); _sizes[0] = 0
    main = lab == np.argmax(_sizes)          # the open lake: one connected body of water
prepare()
def snap(rc):
    idx = np.argwhere(main)
    k = np.argmin(((idx - np.array(rc)) ** 2).sum(1))
    return tuple(idx[k]), math.dist(rc, idx[k]) * cell_m
def line_clear(a, b):
    n = int(max(abs(a[0] - b[0]), abs(a[1] - b[1]))) + 1
    rr = np.linspace(a[0], b[0], n).round().astype(int); cc = np.linspace(a[1], b[1], n).round().astype(int)
    return ok[rr, cc].all()
def simplify(path):
    out = [path[0]]; i = 0
    while i < len(path) - 1:
        j = len(path) - 1
        while j > i + 1 and not line_clear(path[i], path[j]): j -= 1
        out.append(path[j]); i = j
    return out
pts = CFG["points"]; name = CFG["name"]
def route():
    cells, notes = [], []
    for p in pts:
        c, moved = snap(to_cell(p["lat"], p["lon"]))
        cells.append(c); notes.append(f'{p["name"]}: moved {moved:.0f} m to open water')
    full = []; seg_up = []
    for a, b in zip(cells, cells[1:]):
        m = MCP_Geometric(cost, fully_connected=True)
        m.find_costs([a], [b])
        seg = m.traceback(b)
        add = seg if not full else seg[1:]
        full += add; seg_up += [b[0] < a[0]] * len(add)   # on Lough Ree, northward is upstream
    return cells, notes, full, seg_up
# Mark-side rule, IALA A as used on the Shannon: heading upstream, red to port
# and green to starboard; downstream the other way round. Marks within
# END_M of a start, stop or via point are arrival marks and go on the
# hand-check list instead.
END_M = 150
SIDE_M = float(CFG.get("side_m", 200))   # a mark this close on the wrong side means we are inside it
def check_sides(cells, full, seg_up):
    sides = []
    fa = np.array(full, float); tfull = cKDTree(fa)
    for mk in MARKS:
        if "rc" not in mk or mk.get("gauge"): continue
        dd, i = tfull.query(np.array(mk["rc"], float))
        if dd * cell_m > SIDE_M: continue
        i0, i1 = max(0, i - 4), min(len(full) - 1, i + 4)
        dr, dc = fa[i1] - fa[i0]
        vr, vc = np.array(mk["rc"], float) - fa[i]
        cross = dc * (-vr) - (-dr) * vc            # >0: mark on the left (port)
        side = "port" if cross > 0 else "starboard"
        up = seg_up[min(i, len(seg_up) - 1)]
        want = ("port" if mk["kind"] == "red" else "starboard") if up else ("starboard" if mk["kind"] == "red" else "port")
        near_end = min(math.dist(mk["rc"], c) for c in cells) * cell_m <= END_M
        sides.append({"kind": mk["kind"], "lat": mk["lat"], "lon": mk["lon"], "side": side, "want": want,
                      "ok": side == want, "arrival": near_end, "dist_m": round(dd * cell_m),
                      "upstream": bool(up), "_mk": mk, "_i": int(i)})
    return sides
def close_wrong_side(mk, p):
    # block from the mark through the water the route used, to the next hazard
    r, c = mk["rc"]; dr, dc = p[0] - r, p[1] - c
    L = math.hypot(dr, dc) or 1.0; dr, dc = dr / L, dc / L
    k = 0
    while k * cell_m < 400:
        rr, cc = int(round(r + dr * k)), int(round(c + dc * k))
        if not (0 <= rr < G.shape[0] and 0 <= cc < G.shape[1]): break
        if k > 0 and blocked[rr, cc] and k * cell_m > math.dist((r, c), p) * cell_m: break
        blocked[rr, cc] = True
        if rr + 1 < G.shape[0]: blocked[rr + 1, cc] = True
        if cc + 1 < G.shape[1]: blocked[rr, cc + 1] = True
        k += 1
conflicts = []
for it in range(6):
    cells, notes, full, seg_up = route()
    print("   ", "; ".join(n for n in notes if not n.endswith(" 0 m to open water")) or "all points on open water")
    sides = check_sides(cells, full, seg_up)
    bad = [x for x in sides if not x["ok"] and not x["arrival"]]
    print(f"pass {it + 1}: marks passed within {SIDE_M:.0f} m: {len(sides)}; on the wrong side: {len(bad)}")
    if not bad or it == 5: break
    for x in bad:
        # if the right side of this mark can't be reached at all, the rule
        # can't be met here: don't cut the waterway, flag the mark instead
        # (it may be a bridge height gauge, which is drawn the same way)
        keep = blocked.copy()
        close_wrong_side(x["_mk"], full[x["_i"]]); prepare()
        lab_, _ = ndimage.label(ok, structure=np.ones((3, 3)))
        if len({lab_[c] for c in cells}) > 1 or any(lab_[c] == 0 for c in cells):
            blocked[:] = keep; prepare()
            x["_mk"]["gauge"] = True; conflicts.append(x)
            print("   can't pass", x["kind"], round(x["lat"], 5), round(x["lon"], 5), "on its", x["want"], "side without leaving the water: flagged")
        else:
            print("   closing wrong side of", x["kind"], round(x["lat"], 5), round(x["lon"], 5), "passed to", x["side"])
    prepare()
for x in sides: x.pop("_mk"); x.pop("_i")
# simplify each leg on its own so that every start, stop and via point stays
wps = []
_idx = [0] + [int(np.argmin([math.dist(p, c) for p in full])) for c in cells[1:-1]] + [len(full) - 1]
for i0, i1 in zip(_idx, _idx[1:]):
    leg = simplify(full[i0:i1 + 1])
    wps += leg if not wps else leg[1:]
length = sum(math.dist(p, q) for p, q in zip(full, full[1:])) * cell_m
mind = min(dist[r, c] for r, c in full)
# hand-check list: unreadable or part-read figures within 30 m of the path
check = []
fp = np.array(full, float) + 0.5
tp = cKDTree(fp)
for x in _S["doubtful"] + _floor:
    q = np.array([(x["py"] - py0) / B, (x["px"] - px0) / B])
    dcell, _ = tp.query(q)
    if dcell * cell_m <= 30:
        la, lo = to_ll(q[0] - 0.5, q[1] - 0.5)
        check.append({"text": x["text"], "lat": la, "lon": lo, "dist_m": round(dcell * cell_m),
                      "read_as": (f'read as {int(x["depth_ft"])}\'{round((x["depth_ft"] % 1) * 12)}" (stray mark ignored)' if x.get("reread") else (f'at least {x["depth_ft"]:.0f} ft' if x.get("floor") else "unreadable"))})
res = {"conflicts": [{k: v for k, v in x.items() if not k.startswith("_")} for x in conflicts], "locks": [{k: v for k, v in L.items() if k != "rc"} for L in LOCKS], "marks": sides, "check": check, "name": name, "waypoints": [to_ll(*w) for w in wps], "path": [to_ll(*p) for p in full[::3]],
       "length_km": length / 1000, "min_clear_m": mind, "notes": notes, "cell_m": cell_m}
json.dump(res, open(os.path.join(OUT, name + ".json"), "w"))
# GPX for OpenCPN
gpx = ['<?xml version="1.0" encoding="UTF-8"?>', '<gpx version="1.1" creator="Shannon Depth (first look)" xmlns="http://www.topografix.com/GPX/1/1">',
       f'<rte><name>Possible route: {name} (not an aid to navigation)</name>']
for i, (la, lo) in enumerate(res["waypoints"]):
    gpx.append(f'<rtept lat="{la:.6f}" lon="{lo:.6f}"><name>PR{i:02d}</name></rtept>')
gpx += ['</rte>', '</gpx>']
open(os.path.join(OUT, name + ".gpx"), "w").write("\n".join(gpx))
RESULT = res
print(json.dumps({k: res[k] for k in ("length_km", "min_clear_m", "notes", "check")}), len(res["waypoints"]), "waypoints")
