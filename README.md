# Shannon Depth for OpenCPN

An OpenCPN plugin that shows how far today's water level is above or below
the CSIG chart datum for the stretch of the Shannon the boat is in. It reuses
the method, datums and live data of the Shannon Chart Depth page, so the two
always agree.

**Untested experiment, not an aid to navigation.**

## Status

Track A, phase 1 (groundwork): a skeleton plugin that loads in OpenCPN and
draws a placeholder panel on the chart. No live data yet.

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
