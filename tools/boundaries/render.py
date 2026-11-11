# Render a piece of the CSIG chart with stretch polygons and lock zones drawn on.
# usage: python3 render.py spec.json out.png
import json, math, sqlite3, sys, io
from PIL import Image, ImageDraw, ImageFont
spec = json.load(open(sys.argv[1]))
z = spec["zoom"]; (lat0, lon0, lat1, lon1) = spec["bbox"]
n = 2 ** z
def px(lat, lon):
    x = (lon + 180) / 360 * n * 256
    y = (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n * 256
    return x, y
x0, y0 = px(lat1, lon0); x1, y1 = px(lat0, lon1)
tx0, ty0, tx1, ty1 = int(x0 // 256), int(y0 // 256), int(x1 // 256), int(y1 // 256)
db = sqlite3.connect("file:" + spec["chart"] + "?mode=ro", uri=True)
img = Image.new("RGB", ((tx1 - tx0 + 1) * 256, (ty1 - ty0 + 1) * 256), (230, 230, 230))
for tx in range(tx0, tx1 + 1):
    for ty in range(ty0, ty1 + 1):
        row = db.execute("select tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?", (z, tx, n - 1 - ty)).fetchone()
        if row:
            img.paste(Image.open(io.BytesIO(row[0])).convert("RGB"), ((tx - tx0) * 256, (ty - ty0) * 256))
ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(ov)
def P(lat, lon):
    x, y = px(lat, lon); return (x - tx0 * 256, y - ty0 * 256)
cols = {"derg": (230, 60, 60), "mid": (40, 160, 40), "ree": (40, 90, 230), "key": (200, 120, 0)}
for sid, poly in spec.get("polygons", {}).items():
    c = cols.get(sid, (120, 0, 160))
    pts = [P(a, b) for a, b in poly]
    d.polygon(pts, fill=c + (45,), outline=c + (255,))
    d.line(pts + [pts[0]], fill=c + (255,), width=3)
mpp = 156543.03 * math.cos(math.radians((lat0 + lat1) / 2)) / n
for z_ in spec.get("zones", []):
    x, y = P(z_["lat"], z_["lon"]); r = z_["r"] / mpp
    d.ellipse((x - r, y - r, x + r, y + r), outline=(0, 0, 0, 255), width=3, fill=(255, 255, 0, 70))
    d.text((x + r + 3, y - 6), z_.get("name", ""), fill=(0, 0, 0, 255))
img = Image.alpha_composite(img.convert("RGBA"), ov).convert("RGB")
cx0, cy0 = P(lat1, lon0); cx1, cy1 = P(lat0, lon1)
img = img.crop((int(cx0), int(cy0), int(cx1), int(cy1)))
if spec.get("maxw") and img.width > spec["maxw"]:
    img = img.resize((spec["maxw"], int(img.height * spec["maxw"] / img.width)))
img.save(sys.argv[2])
print(img.size)
