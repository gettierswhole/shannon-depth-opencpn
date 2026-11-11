# Shannon Depth for OpenCPN

An OpenCPN plugin that shows how far today's water level is above or below
the CSIG chart datum for the stretch of the Shannon the boat is in. It reuses
the method, datums and live data of the Shannon Chart Depth page, so the two
always agree.

**Untested experiment, not an aid to navigation.**

## Status

Track A, phase 3 in progress. The panel on the chart shows, for the stretch
the boat is in (or the chart centre when there is no GPS fix):

- the correction to charted depths in feet and inches, with its band,
  rounded toward less water, as on the page
- what to do with it ("Add 1 ft 4 in to charted depths")
- the reading's age: green under 3 h, amber under 12 h, red "Out of date"
  after that, and greyed out with "use your sounder" after 48 h
- "estimated" on the river stretches, and the Killaloe sill caveat on Lough Derg
- "No stretch" outside the four stretches, and no figure within a few hundred
  metres of a lock or weir

It fetches live.json and datums.json from the Shannon Chart Depth site every
15 minutes and keeps the last good copies for use offline.

## Install a test build

1. Open the latest successful run under **Actions → build**.
2. Download the artifact for your OpenCPN build (`ShannonDepth-windows-x64`
   or `ShannonDepth-windows-Win32`) and unzip it. It contains a `.tar.gz`
   package and its `.xml` metadata.
3. In OpenCPN: **Options → Plugins → Import plugin…**, pick the `.tar.gz`,
   then enable *ShannonDepth*.

## Building

Built from OpenCPN's shipdriver plugin template. See `INSTALL.md` for local
builds. Windows packages are built by GitHub Actions
(`.github/workflows/build.yml`).

## Data

The plugin reads only the public `live.json` and `datums.json` files of the
Shannon Chart Depth site. CSIG chart data is never stored in this repository.

## Licence

GPL v3 or later, as the template.
