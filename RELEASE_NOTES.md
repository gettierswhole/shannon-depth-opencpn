**Untested experiment, not an aid to navigation.** Pre-release for the owner's testing.

Shows, on the OpenCPN chart, how far today's Shannon water level is above or below the CSIG chart datum for the stretch the boat is in, using the live gauges and datums of the Shannon Chart Depth page.

**In this version (0.2.0)**
- Correction panel: the correction to charted depths in feet and inches with its band, what to do with it, the reading's age (green under 3 h, amber under 12 h, red "Out of date" after, greyed out with "use your sounder" after 48 h), and the gauge with its trend.
- Stretch chosen from the boat's GPS fix, or the chart centre when there is no fix.
- "Estimated" on the river stretches (Meelick–Athlone, Carrick pool); Killaloe sill caveat on Lough Derg.
- "No stretch" outside the four stretches; no figure within a few hundred metres of a lock or weir.
- Fetches live.json and datums.json every 15 minutes; keeps the last good copies for use offline.

**Not yet**: depth calculator, entering your own reading, settings.

**Packages**
- Windows 32-bit (OpenCPN's usual Windows build): `…msvc-wx32-10-x86.tar.gz`
- Windows 64-bit: `…msvc-64-10-x86.tar.gz`
- Android arm64: `…android-arm64-16-arm64.tar.gz`

Install on Windows with Options → Plugins → Import plugin… On Android, use a custom plugin catalog pointing at this release's `ocpn-plugins.xml`.
