"""Possible route: read the member's own chart once, then plan routes on it.

Everything read from the chart stays in this computer's cache folder. Nothing
is sent anywhere; the only thing fetched is today's gauge readings.
"""
import os, sys, json, hashlib, runpy, io, contextlib, time
# The engine scripts are run from source; import what they use here so a
# frozen (PyInstaller) build carries it.
import numpy, scipy.ndimage, scipy.spatial, skimage.graph, PIL.Image  # noqa: F401
HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.join(HERE, "engine")
# the engine scripts import their helpers as top-level modules; share them
if ENGINE not in sys.path: sys.path.insert(0, ENGINE)
import tiles, grid  # noqa: E402
from . import levels as _levels, places, draw as _draw  # noqa: E402
DATA = os.path.join(HERE, "data")
VERSION = "0.1.0-beta"
ENGINE_REV = "1"   # bump to rebuild every member's cache after an extraction change


def app_dir():
    base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~/.local/share")
    p = os.path.join(base, "ShannonPossibleRoute")
    os.makedirs(p, exist_ok=True)
    return p


def out_dir():
    docs = os.path.join(os.path.expanduser("~"), "Documents")
    p = os.path.join(docs if os.path.isdir(docs) else os.path.expanduser("~"), "Possible Route")
    os.makedirs(p, exist_ok=True)
    return p


def chart_key(chart):
    st = os.stat(chart)
    return hashlib.sha1(f"{os.path.basename(chart)}|{st.st_size}|{int(st.st_mtime)}|{ENGINE_REV}".encode()).hexdigest()[:12]


def cache_dir(chart, area=places.AREA):
    p = os.path.join(app_dir(), "cache", chart_key(chart), area["id"])
    os.makedirs(p, exist_ok=True)
    return p


def _run(script, cfg, log):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        runpy.run_path(os.path.join(ENGINE, script), init_globals={"CFG": cfg}, run_name="__engine__")
    for line in buf.getvalue().splitlines():
        log(line)
    return buf.getvalue()


def check_chart(chart):
    import sqlite3
    con = sqlite3.connect("file:" + chart + "?mode=ro", uri=True)
    try:
        n = con.execute("select count(*) from tiles where zoom_level=16").fetchone()[0]
        meta = dict(con.execute("select name, value from metadata").fetchall()) if con.execute(
            "select count(*) from sqlite_master where name='metadata'").fetchone()[0] else {}
    finally:
        con.close()
    if n == 0:
        raise ValueError("This chart has no zoom-16 tiles. Possible route needs the CSIG Inland Waterways chart (.mbtiles).")
    return meta


def prepare(chart, log=print, force=False):
    """Read the chart for the area once. Takes a few minutes the first time."""
    check_chart(chart)
    area = places.AREA
    cd = cache_dir(chart, area)
    done = os.path.join(cd, "ready.json")
    if os.path.exists(done) and not force:
        return cd
    cfg = {"chart": chart, "cache": cd, "bbox": area["bbox"],
           "datums": os.path.join(DATA, "datums.json"), "boundaries": os.path.join(DATA, "boundaries.json")}
    t0 = time.time()
    log(f"Reading {area['name']} from your chart (first time only, a few minutes)…")
    tiles.open_chart(chart)
    def step(n, label, outfile, fn):
        if os.path.exists(os.path.join(cd, outfile)):
            return          # done on an earlier run that was interrupted later
        log(f"{n}/5 {label}…")
        fn()
    def build_grid():
        with contextlib.redirect_stdout(io.StringIO()):
            grid.build(*area["bbox"], os.path.join(cd, "grid.npz"))
    step(1, "Water, shading and land", "grid.npz", build_grid)
    step(2, "Characters on the chart", "glyphs.npz", lambda: _run("glyphs.py", cfg, log))
    step(3, "Soundings", "soundings.json", lambda: _run("soundings.py", cfg, log))
    _sanity_soundings(cd)
    step(4, "Marks", "marks.json", lambda: _run("marks.py", cfg, log))
    step(5, "Locks and weirs", "locks.json", lambda: _run("lockpass.py", cfg, log))
    json.dump({"version": VERSION, "engine": ENGINE_REV, "seconds": round(time.time() - t0)}, open(done, "w"))
    log(f"Chart read in {round(time.time() - t0)} s. This won't be needed again for this chart.")
    return cd


def _sanity_soundings(cd):
    s = json.load(open(os.path.join(cd, "soundings.json")))
    ok, bad = len(s["soundings"]), len(s["doubtful"])
    if ok < 1000 or ok < 3 * bad:
        raise RuntimeError(f"Only {ok} depth figures could be read ({bad} set aside). This looks like a different "
                           "chart or edition from the one the beta was built for, so no route will be offered.")


def find(chart, points, draught_ft, margin_ft, name=None, lev=None, log=print):
    """points: list of {"name", "lat", "lon"}. Returns the result dict and file paths."""
    cd = prepare(chart, log=log)
    if lev is None:
        log("Fetching today's gauge readings…")
        lev = _levels.fetch()
    name = name or " - ".join(p["name"].split(",")[0] for p in (points[0], points[-1]))
    safe = "".join(c for c in name if c not in '<>:"/\\|?*').strip() or "Possible route"
    out = out_dir()
    cfg = {"chart": chart, "cache": cd, "out": out, "datums": os.path.join(DATA, "datums.json"),
           "boundaries": os.path.join(DATA, "boundaries.json"), "levels": lev,
           "draught_ft": draught_ft, "margin_ft": margin_ft, "points": points, "name": safe}
    tiles.open_chart(chart)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        g = runpy.run_path(os.path.join(ENGINE, "route.py"), init_globals={"CFG": cfg}, run_name="__engine__")
    res = g["RESULT"]
    res["log"] = buf.getvalue().splitlines()
    res["levels"] = lev
    res["draught_ft"], res["margin_ft"] = draught_ft, margin_ft
    png = os.path.join(out, safe + ".png")
    _draw.draw(res, png)
    json.dump(res, open(os.path.join(out, safe + ".json"), "w"), indent=1)
    return res, {"gpx": os.path.join(out, safe + ".gpx"), "png": png, "json": os.path.join(out, safe + ".json")}


def summary(res):
    """Plain-language report of a route, in feet and inches."""
    def ftin(x):
        i = int(round(x * 12)); return f"{i // 12}'{i % 12}\""
    L = [f"Possible route, not an aid to navigation.",
         f"{res['length_km']:.1f} km ({res['length_km'] / 1.852:.1f} nautical miles), {len(res['waypoints'])} waypoints.",
         f"Draught {ftin(res['draught_ft'])}, margin {ftin(res['margin_ft'])} under the keel."]
    for n in res["notes"]:
        if not n.endswith(" 0 m to open water"):
            L.append("Moved: " + n + (" — the place may be cut off at today's level with this draught and margin."
                                      if "moved" in n and int(n.split("moved ")[1].split(" ")[0]) > 300 else ""))
    import math
    for k in res.get("locks", []):
        near = min(math.hypot((p[0] - k["lat"]) * 111000, (p[1] - k["lon"]) * 66000) for p in res["path"])
        if k["cills"] and near < 300:
            L.append(f"{k['name']}: " + ", ".join(f"{c['cill']} {ftin(c['over_ft'])} of water" for c in k["cills"])
                     + ("" if k["ok"] else " — NOT ENOUGH for this boat"))
    marks = res.get("marks", [])
    good = sum(m["ok"] for m in marks)
    L.append(f"Marks passed: {good} of {len(marks)} on the correct side (red to port going upstream).")
    for m in marks:
        if not m["ok"]:
            L.append(f"  Check by eye: {m['kind']} mark at {m['lat']:.5f}, {m['lon']:.5f} passed to {m['side']}"
                     + (" (arriving or leaving a quay)" if m.get("arrival") else ""))
    for m in res.get("conflicts", []):
        L.append(f"  Check by eye: {m['kind']} at {m['lat']:.5f}, {m['lon']:.5f} could not be kept to {m['want']} "
                 "— possibly a bridge height gauge, which is drawn like a mark.")
    if res.get("check"):
        L.append(f"Depth figures near the route that could not be read cleanly ({len(res['check'])}):")
        for c in res["check"]:
            L.append(f"  {c['text']} at {c['lat']:.5f}, {c['lon']:.5f}, {c['dist_m']} m off: {c['read_as']}")
    return "\n".join(L)
