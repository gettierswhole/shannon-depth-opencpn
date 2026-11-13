# Classify the CSIG chart into a navigation grid for one area.
# Cell = 8x8 pixels at zoom 16 (about 11 m on Lough Ree).
# Classes: 0 water, 1 shallow (blue shading), 2 very shallow (red shading),
#          3 land / unsurveyed / off chart, 4 mark symbol (green or red symbol)
import sys, os, io, sqlite3, math, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import tiles
Z, B = 16, 8
def classify(a):
    r, g, b = [a[..., i].astype(int) for i in range(3)]
    water = (r >= 244) & (g >= 244) & (b >= 244) & ~((r == 255) & (g == 255) & (b == 255))
    dark = (r < 90) & (g < 90) & (b < 90)               # text
    blue = (b > 200) & (b - r > 40)                     # shallow shading
    red = (r > 200) & (g < 90) & (b < 90)               # very shallow shading / red marks
    green = (g > 120) & (r < 100) & (b < 120)           # green marks
    landy = ((r > 190) & (g > 140) & (b < 80)) | ((r > 100) & (r < 160) & (g > 100) & (g < 160) & (b < 40))
    grey = (abs(r - g) < 6) & (abs(g - b) < 6) & (r >= 180) & (r <= 235)
    return water, dark, blue, red, green, landy, grey
def build(lat0, lon0, lat1, lon1, out):
    x0, y0 = tiles.tile_xy(lat1, lon0, Z); x1, y1 = tiles.tile_xy(lat0, lon1, Z)
    tx0, ty0, tx1, ty1 = int(x0), int(y0), int(x1), int(y1)
    n = 2 ** Z; per = 256 // B
    G = np.full(((ty1 - ty0 + 1) * per, (tx1 - tx0 + 1) * per), 3, np.uint8)
    for tx in range(tx0, tx1 + 1):
        for ty in range(ty0, ty1 + 1):
            row = tiles.CHART.execute("select tile_data from tiles where zoom_level=? and tile_column=? and tile_row=?", (Z, tx, n - 1 - ty)).fetchone()
            if not row: continue
            a = np.asarray(Image.open(io.BytesIO(row[0])).convert("RGB"))
            water, dark, blue, red, green, landy, grey = classify(a)
            white = (a[..., 0] == 255) & (a[..., 1] == 255) & (a[..., 2] == 255)
            def blk(m): return m.reshape(per, B, per, B).sum(axis=(1, 3))
            W, D, BL, R, GR, L, GY, WH = map(blk, (water, dark, blue, red, green, landy, grey, white))
            r_, g_, b_ = [a[..., i].astype(int) for i in range(3)]
            # Anti-aliased text edges are greys between black and water: count
            # everything that is not land, shading, mark colour or no-data grey
            # as water when the cell is mostly water already.
            neutral = (abs(r_ - g_) < 12) & (abs(g_ - b_) < 12) & ~grey & ~((r_ == 255) & (g_ == 255) & (b_ == 255))
            N = blk(neutral)
            cell = np.full((per, per), 3, np.uint8)
            cell[((W + D + N) >= B * B * 0.85) & (W >= B * B * 0.25)] = 0
            # light blue: about 4.5 to 10 ft charted; darker blue: about 2.5 to 6 ft
            mid_blue = (b_ > 200) & (r_ < 150) & (b_ - r_ > 40)
            MB = blk(mid_blue)
            cell[(BL >= 6) & (cell != 3)] = 1
            cell[(BL >= 6) & (W + BL + D >= B * B * 0.6)] = 1
            cell[(MB >= 6) & ((cell == 1) | (cell == 0))] = 6
            cell[R >= 24] = 2      # solid red fill; thin red lines (cables) are not shallows
            cell[GR >= 4] = 4
            cell[(L + GY) >= B * B * 0.5] = 3
            cell[(cell == 3) & ((WH + D) >= B * B * 0.85) & (WH >= B * B * 0.25)] = 5   # white: decided below
            G[(ty - ty0) * per:(ty - ty0 + 1) * per, (tx - tx0) * per:(tx - tx0 + 1) * per] = cell
    from scipy import ndimage
    # White counts as water only where it joins known water (some lake tiles
    # have a pure white background; so does the area outside the chart).
    lab, _ = ndimage.label((G == 0) | (G == 5), structure=np.ones((3, 3)))
    wet = np.unique(lab[G == 0]); wet = wet[wet > 0]
    G[(G == 5) & np.isin(lab, wet)] = 0
    G[G == 5] = 3
    # Blue that is only a speck or a thin line (the apostrophe beside each
    # sounding, dashed boundary lines) is not a shallow area.
    sh = (G == 1) | (G == 6)
    solid = ndimage.binary_opening(sh, structure=np.ones((3, 3)))
    G[sh & ~ndimage.binary_dilation(solid, structure=np.ones((3, 3)))] = 0
    np.savez_compressed(out, grid=G, px0=tx0 * 256, py0=ty0 * 256, zoom=Z, cell=B)
    print(out, G.shape, {k: int((G == k).sum()) for k in range(5)})
