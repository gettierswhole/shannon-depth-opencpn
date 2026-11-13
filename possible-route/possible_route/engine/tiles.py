import math, sqlite3, io
from PIL import Image
CHART = None
def open_chart(p):
    global CHART
    CHART = sqlite3.connect("file:" + p + "?mode=ro", uri=True)
def tile_xy(lat, lon, z):
    n = 2 ** z
    x = (lon + 180) / 360 * n
    y = (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n
    return x, y
def pix_ll(px, py, z):
    n = 2 ** z * 256
    lon = px / n * 360 - 180
    lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * py / n))))
    return lat, lon
def mosaic(lat0, lon0, lat1, lon1, z):
    x0, y0 = tile_xy(lat1, lon0, z); x1, y1 = tile_xy(lat0, lon1, z)
    tx0, ty0, tx1, ty1 = int(x0), int(y0), int(x1), int(y1)
    n = 2 ** z
    img = Image.new("RGB", ((tx1 - tx0 + 1) * 256, (ty1 - ty0 + 1) * 256), (230, 230, 230))
    for tx in range(tx0, tx1 + 1):
        for ty in range(ty0, ty1 + 1):
            r = CHART.execute("select tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?", (z, tx, n - 1 - ty)).fetchone()
            if r:
                img.paste(Image.open(io.BytesIO(r[0])).convert("RGB"), ((tx - tx0) * 256, (ty - ty0) * 256))
    return img, tx0 * 256, ty0 * 256

def contains(poly, pts):
    """Even-odd test: which of pts (N x 2, lat/lon) are inside poly (list of [lat, lon])."""
    import numpy as np
    P = np.asarray(poly, float); x, y = pts[:, 1], pts[:, 0]
    inside = np.zeros(len(pts), bool)
    xj, yj = P[-1, 1], P[-1, 0]
    for xi, yi in zip(P[:, 1], P[:, 0]):
        cross = ((yi > y) != (yj > y))
        with np.errstate(divide="ignore", invalid="ignore"):
            xint = (xj - xi) * (y - yi) / (yj - yi) + xi
        inside ^= cross & (x < xint)
        xj, yj = xi, yi
    return inside
