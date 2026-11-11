# Stretch boundaries

`boundaries.json` holds the four stretch polygons (lat, lon) and the lock
zones. Each polygon edge crosses the navigation only inside a lock zone, so
a boat is never placed on the wrong side of a lock. Inside a zone the plugin
shows no figure.

Lock and weir positions come from OpenStreetMap (© OpenStreetMap
contributors, ODbL). The polygons were drawn by hand and checked against the
CSIG chart with `render.py`, which draws them over the .mbtiles chart:

    python3 render.py spec.json out.png

`spec.json` gives the chart path, zoom, bbox [lat0, lon0, lat1, lon1],
polygons and zones. The CSIG chart itself is never stored in this repository.

Rebuild the plugin's copy of the datums after any change:

    node tools/export_datums.js <path to the page's index.html> \
        tools/boundaries/boundaries.json > data/datums.json
    python3 tests/gen_boundaries.py tools/boundaries/boundaries.json > tests/boundary_data.h
