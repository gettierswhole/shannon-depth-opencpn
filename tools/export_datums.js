#!/usr/bin/env node
// Export datums.json from the Shannon Chart Depth page, so the plugin and the
// page share one set of figures.
//
// Usage: node tools/export_datums.js <index.html> [boundaries.json] > datums.json
//
// Reads the page's REACHES table (stretch, gauge, cills, feet over the cill,
// summer level, uncertainty, caveats) and works out each datum exactly as the
// page's datumOf() does with default settings.
"use strict";
const fs = require("fs");
const vm = require("vm");

const [page, boundaryFile] = process.argv.slice(2);
if (!page) { console.error("usage: export_datums.js <index.html> [boundaries.json]"); process.exit(2); }
const html = fs.readFileSync(page, "utf8");

function grab(re, what) {
  const m = html.match(re);
  if (!m) { console.error("cannot find " + what + " in " + page); process.exit(1); }
  return m[0];
}
const consts = grab(/var FT = [^;]*POOLBEG_TO_MALIN = [^;]*;/, "unit constants");
const reaches = grab(/var REACHES = \[[\s\S]*?\n  \];\n  REACHES\.forEach\([\s\S]*?\n  \}\);/, "REACHES table");

const box = {};
vm.runInNewContext(consts + "\n" + reaches + "\nout.REACHES = REACHES; out.FOOT = FOOT; out.P2M = POOLBEG_TO_MALIN;", { out: box });

const r3 = (x) => Math.round(x * 1000) / 1000;
const boundaries = boundaryFile ? JSON.parse(fs.readFileSync(boundaryFile, "utf8")) : {};

const stretches = box.REACHES.map((r) => {
  let datum = null, basis = null, how = null;
  if (r.feet != null && r.cills.length) {
    const c = r.cills[0];
    datum = c.malin + r.feet * box.FOOT;  // unrounded, as the page uses it
    basis = "chart-datum";
    how = r.feet + " ft over the " + c.name + " (" + c.malin.toFixed(3) + " m OD Malin)";
  } else if (r.osl) {
    datum = r.osl.malin;
    basis = "summer-level";
    how = "Waterways Ireland " + r.osl.name + " (" + r.osl.malin.toFixed(3) + " m OD Malin)";
  }
  const alternatives = (r.alts || []).map((a) => ({
    label: a.label,
    level: r.cills[a.cill].malin + a.feet * box.FOOT,
  }));
  return {
    id: r.id,
    name: r.name,
    span: r.span,
    gauge: { doc: r.gauge.doc, station: r.gauge.station, name: r.gauge.name },
    datum: datum == null ? null : { level: datum, basis, how, confirmed: false },
    alternatives,
    band: r.unc,
    bandWhy: r.uncWhy || "",
    estimated: basis === "summer-level",
    lake: !!r.lake,
    caveats: r.caveats || [],
    boundary: boundaries[r.id] || null,
  };
});

process.stdout.write(JSON.stringify({
  schema: 1,
  generated: new Date().toISOString().replace(/\.\d+Z$/, "Z"),
  units: "metres, OD Malin",
  notice: "Untested experiment, not an aid to navigation.",
  stretches,
  // Near a lock or weir the level changes across a few metres, so the plugin
  // shows no figure inside these circles (radius in metres).
  lockZones: boundaries.zones || [],
}, null, 2) + "\n");
