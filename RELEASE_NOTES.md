**Untested experiment, not an aid to navigation.** Pre-release for the owner's testing.

Shows, on the OpenCPN chart, how far today's Shannon water level is above or below the CSIG chart datum for the stretch the boat is in, using the live gauges and datums of the Shannon Chart Depth page.

**New in 0.3.0**
- Toolbar button opening three tabs: **Calculator**, **Your reading** and **Settings** (also from Options → Plugins → Preferences).
- **Calculator**: enter the charted depth; get the water there now as a range and the clearance under the keel judged on the shallow end, with the page's verdicts (Should be fine, Go slow, Too tight, Could touch). Your draught is remembered.
- **Your reading**: a gauge level in m OD Malin or a staff-gauge reading, feet over a lock cill, or a sounder / lead-line check at a charted spot. It replaces the gauge for that stretch, with the live gauge figure shown beside it. It stays in use while the official gauge stays within half the stretch's band of its level when you entered it (up to 7 days, and only while the gauge is current); otherwise it turns red at 3 h and lapses at 12 h.
- **Settings**: draught, panel corner, choosing the stretch by hand, and the datum choices the page offers (Lough Derg upper or lower Killaloe sill, with a warning; Lough Ree 5 ft 6 in, 7 ft or 5 ft over the Athlone cill).

**From 0.2.0**: the live correction panel (correction in feet and inches with its band, age and freshness colour, "estimated" river stretches, Killaloe sill caveat, no figure near locks, offline cache).

Install on Windows from the custom catalog, or with Options → Plugins → Import plugin… (the `msvc-wx32` package for the usual 32-bit OpenCPN).
