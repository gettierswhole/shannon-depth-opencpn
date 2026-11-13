# Possible route (beta) — Lough Ree

**Possible route, not an aid to navigation.** It suggests a way between two or more places, worked out from your own copy of the CSIG Inland Waterways chart, today's gauge readings and your boat's draught. Check every route against the chart and what you can see before you follow it.

Beta. Lough Ree only, from Athlone (including through Athlone lock) to Lanesborough.

## What you need

- A Windows 10 or 11 PC.
- Your own copy of the CSIG Inland Waterways chart, the `.mbtiles` file you use in OpenCPN.
- An internet connection, to fetch today's water levels.

## Install

1. Download `PossibleRoute-windows.zip` from the latest **Possible route** release on this repository's Releases page.
2. Unzip it anywhere, for example into `Documents\PossibleRoute`.
3. Run `PossibleRoute.exe`. Windows may warn that it is from an unknown publisher, because the beta isn't signed. Choose **More info → Run anyway**.

## Use

1. **Browse…** to your chart file. It's remembered next time.
2. Enter your **draught** and the **margin** you want under the keel, in feet and inches, for example `3'3"` and `2'0"`.
3. Choose the start, any stops and the finish. Use **Other** to type a position as latitude, longitude.
4. Click **Find a possible route**.

The first time, it reads Lough Ree from your chart. That takes a few minutes, and it won't be needed again for the same chart.

The results go in `Documents\Possible Route`:

- **`<name>.gpx`**: in OpenCPN, open Route & Mark Manager and choose **Import GPX**.
- **`<name>.png`**: the route drawn over your chart.
- **`<name>.json`**: the full details, including the hand-check list.

## What it does

- **Depth.** Each stretch uses its own gauge: today's level, less the chart datum, less the stretch's uncertainty band. A cell of water is used only if the charted depth there, plus that correction, gives your draught plus margin. Shaded water counts as deep enough only where the soundings around it say so.
- **Marks.** Red to port going upstream and green to starboard (IALA A, as on the Shannon). Downstream it's the other way round. Marks right at a quay are listed for you to judge.
- **Locks.** A lock is passable when the water over each cill, on its own side's gauge, gives your draught plus margin. Weirs are walls. The way through is the lock chamber.
- **Hand-check list.** It names every depth figure near the route that couldn't be read cleanly, every mark you need to judge, and any green that couldn't be kept to starboard. Those are often bridge height gauges, which the chart draws like a mark.

## Privacy

Your chart is read on your PC only. What's read from it is kept in `%LOCALAPPDATA%\ShannonPossibleRoute`: the water grid, the depth figures and the marks. It is never uploaded. The only thing fetched from the internet is the public gauge feed. This repository contains code only, and no chart data.

## Problems

If something goes wrong, the details are saved in `%LOCALAPPDATA%\ShannonPossibleRoute\last-error.txt`. Please send that file with your report.
