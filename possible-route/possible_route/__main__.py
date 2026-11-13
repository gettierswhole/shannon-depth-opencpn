"""Possible route (beta). Run with no arguments for the window, or:
  possible-route --chart CHART --draught 3.25 --margin 2 --via "Hodson Bay" --via "Lanesborough, Ballyleague quay"
  possible-route --selftest
"""
import sys, json, argparse


def cli(argv):
    from possible_route import core, places
    ap = argparse.ArgumentParser(prog="possible-route")
    ap.add_argument("--chart"); ap.add_argument("--draught", type=float, default=3.25)
    ap.add_argument("--margin", type=float, default=2.0)
    ap.add_argument("--via", action="append", default=[], help="place name from the list, or 'lat,lon'; first is the start")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--levels", help="gauge readings as JSON (for testing without the live feed)")
    a = ap.parse_args(argv)
    if a.selftest:
        import numpy, scipy.ndimage, skimage.graph, PIL.Image  # noqa
        if sys.platform == "win32": import tkinter  # noqa
        import tiles
        import numpy as np
        assert tiles.contains([[0, 0], [0, 1], [1, 1], [1, 0]], np.array([[0.5, 0.5], [2, 2]])).tolist() == [True, False]
        print("selftest ok", core.VERSION); return 0
    lookup = {n: (la, lo) for n, la, lo in places.PLACES}
    pts = []
    for v in a.via:
        if v in lookup: pts.append({"name": v, "lat": lookup[v][0], "lon": lookup[v][1]})
        else:
            la, lo = map(float, v.split(",")); pts.append({"name": v, "lat": la, "lon": lo})
    lev = json.load(open(a.levels))["gauge"] if a.levels else None
    res, files = core.find(a.chart, pts, a.draught, a.margin, lev=lev)
    print(core.summary(res)); print(json.dumps(files))
    return 0


def main():
    if len(sys.argv) > 1:
        sys.exit(cli(sys.argv[1:]))
    from possible_route.app import main as gui
    gui()


if __name__ == "__main__":
    main()
