"""Draw a route over the member's own chart, for a quick look before importing the GPX."""
import math
from PIL import ImageDraw
import tiles  # the engine's helper, put on the path by core


def draw(result, out_png, max_px=1600):
    lats = [p[0] for p in result["path"]] + [w[0] for w in result["waypoints"]]
    lons = [p[1] for p in result["path"]] + [w[1] for w in result["waypoints"]]
    pad_lat = max(0.002, (max(lats) - min(lats)) * 0.08)
    pad_lon = max(0.003, (max(lons) - min(lons)) * 0.08)
    lat0, lat1 = min(lats) - pad_lat, max(lats) + pad_lat
    lon0, lon1 = min(lons) - pad_lon, max(lons) + pad_lon
    # pick the most detailed zoom that keeps the picture a sensible size
    z = 16
    while z > 11:
        x0, y0 = tiles.tile_xy(lat1, lon0, z); x1, y1 = tiles.tile_xy(lat0, lon1, z)
        if max(x1 - x0, y1 - y0) * 256 <= max_px * 1.6: break
        z -= 1
    img, ox, oy = tiles.mosaic(lat0, lon0, lat1, lon1, z)
    d = ImageDraw.Draw(img)

    def P(la, lo):
        x, y = tiles.tile_xy(la, lo, z)
        return (x * 256 - ox, y * 256 - oy)

    pts = [P(*w) for w in result["waypoints"]]
    w = 3 if z >= 15 else 2
    d.line(pts, fill=(200, 0, 160), width=w + 2)
    d.line(pts, fill=(255, 60, 220), width=w)
    for p in pts:
        d.ellipse((p[0] - w - 1, p[1] - w - 1, p[0] + w + 1, p[1] + w + 1), outline=(120, 0, 100), width=2)
    x0, y0 = tiles.tile_xy(lat1, lon0, z); x1, y1 = tiles.tile_xy(lat0, lon1, z)
    img = img.crop((int(x0 * 256 - ox), int(y0 * 256 - oy), int(x1 * 256 - ox), int(y1 * 256 - oy)))
    if max(img.size) > max_px:
        k = max_px / max(img.size)
        img = img.resize((int(img.width * k), int(img.height * k)))
    img.save(out_png)
    return out_png
