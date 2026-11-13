"""Today's water levels, from the same live feed the Shannon Depth plugin uses."""
import json, urllib.request

LIVE_URL = "https://leehughes.ie/live.json"


def fetch(timeout=20):
    try:
        return _fetch(timeout)
    except Exception as e:
        raise RuntimeError("Couldn't get today's gauge readings from the live feed "
                           f"({e}). A route depends on today's levels, so none is offered offline.") from e


def _fetch(timeout):
    req = urllib.request.Request(LIVE_URL, headers={"User-Agent": "ShannonPossibleRoute/0.1"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        data = json.load(r)
    out = {}
    for doc, g in data.get("gauge", {}).items():
        v, t = g.get("level"), g.get("time")
        if v is None and g.get("history"):
            v, t = g["history"][-1]["v"], g["history"][-1]["t"]
        if v is not None:
            out[doc] = {"v": float(v), "t": t, "name": g.get("name", doc)}
    return out
