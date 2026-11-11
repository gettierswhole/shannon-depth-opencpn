// Shannon Depth: calculation core. See depth_core.h.

#include "depth_core.h"

#include <algorithm>
#include <cmath>

namespace sd {

namespace {

// JavaScript's Math.round: halves go toward +infinity.
double JsRound(double x) { return std::floor(x + 0.5); }

const char* kMinus = "\xE2\x88\x92";  // U+2212 minus sign

}  // namespace

// ---- Imperial display ---------------------------------------------------

int InchesSafe(double m) {
  return static_cast<int>(std::floor(m / kInch + 1e-6));
}

int InchesNear(double m) { return static_cast<int>(JsRound(m / kInch)); }

int InchesUp(double m) {
  return static_cast<int>(std::ceil(std::fabs(m) / kInch - 1e-6));
}

std::string FtInText(int n) {
  n = std::abs(n);
  int f = n / 12, i = n % 12;
  if (f) return std::to_string(f) + " ft" + (i ? " " + std::to_string(i) + " in" : "");
  return std::to_string(i) + " in";
}

std::string SignedFtIn(int n) {
  return std::string(n < 0 ? kMinus : "+") + FtInText(n);
}

// ---- Stretches ----------------------------------------------------------

bool InsidePolygon(const std::vector<LatLon>& poly, LatLon p) {
  bool inside = false;
  size_t n = poly.size();
  if (n < 3) return false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const LatLon& a = poly[i];
    const LatLon& b = poly[j];
    if ((a.lat > p.lat) != (b.lat > p.lat)) {
      double lon_at = (b.lon - a.lon) * (p.lat - a.lat) / (b.lat - a.lat) + a.lon;
      if (p.lon < lon_at) inside = !inside;
    }
  }
  return inside;
}

const Stretch* StretchAt(const std::vector<Stretch>& stretches, LatLon p) {
  const Stretch* found = nullptr;
  for (const auto& s : stretches) {
    if (!InsidePolygon(s.boundary, p)) continue;
    if (found) return nullptr;  // overlapping polygons: refuse to guess
    found = &s;
  }
  return found;
}

double DistanceMetres(LatLon a, LatLon b) {
  const double r = 6371008.8, k = 3.14159265358979323846 / 180;
  double dlat = (b.lat - a.lat) * k, dlon = (b.lon - a.lon) * k;
  double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
             std::cos(a.lat * k) * std::cos(b.lat * k) * std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * r * std::asin(std::min(1.0, std::sqrt(h)));
}

Location Locate(const std::vector<Stretch>& stretches,
                const std::vector<LockZone>& zones, LatLon p) {
  Location loc;
  for (const auto& z : zones) {
    if (DistanceMetres(z.centre, p) <= z.radius_m) {
      loc.lock = &z;
      return loc;
    }
  }
  loc.stretch = StretchAt(stretches, p);
  return loc;
}

// ---- Readings -----------------------------------------------------------

ManualState ManualStateAt(const ManualReading& m, Millis now) {
  if (!m.valid) return ManualState::kNone;
  Millis age = now - m.time;
  if (age > kManualMax) return ManualState::kLapsed;
  if (age >= kManualRed) return ManualState::kRed;
  return ManualState::kFresh;
}

Freshness FreshnessOf(const Reading& r, Millis now, const FreshnessRules& rules) {
  if (!r.valid) return Freshness::kNoData;
  Millis age = now - r.time;
  if (age < rules.current) return Freshness::kCurrent;
  if (age < rules.ageing) return Freshness::kAgeing;
  if (age < rules.unusable) return Freshness::kOutOfDate;
  return Freshness::kUnusable;
}

// ---- Correction ---------------------------------------------------------

Correction CorrectionOf(const Stretch& s, const Reading& r) {
  Correction c;
  if (!s.has_datum || !r.valid) return c;
  c.available = true;
  c.metres = r.level - s.datum;
  c.inches = InchesSafe(c.metres);
  c.band_inches = InchesUp(s.band);
  c.estimated = s.estimated;
  return c;
}

// ---- Depth calculator ---------------------------------------------------

DepthResult DepthAt(const Stretch& s, const Correction& c, double charted,
                    double draught) {
  DepthResult d;
  if (!c.available || !std::isfinite(charted)) return d;
  double corr = c.metres;
  if (c.estimated && corr > 0) {
    corr = 0;
    d.rise_not_added = true;
  }
  double u = s.band;
  d.available = true;
  d.depth = charted + corr;
  d.low = std::max(0.0, d.depth - u);
  d.high = d.depth + u;
  if (!(draught > 0) || !std::isfinite(draught)) return d;
  d.has_clearance = true;
  d.clearance = d.depth - draught;
  d.worst = d.clearance - u;
  if (d.worst < 0)
    d.verdict = Verdict::kCouldTouch;
  else if (d.worst < kTooTightBelow)
    d.verdict = Verdict::kTooTight;
  else if (d.worst < kGoSlowBelow)
    d.verdict = Verdict::kGoSlow;
  else
    d.verdict = Verdict::kShouldBeFine;
  return d;
}

const char* VerdictText(Verdict v) {
  switch (v) {
    case Verdict::kShouldBeFine: return "Should be fine";
    case Verdict::kGoSlow: return "Go slow";
    case Verdict::kTooTight: return "Too tight";
    case Verdict::kCouldTouch: return "Could touch";
    default: return "";
  }
}

// ---- Trend --------------------------------------------------------------

std::string TrendText(const std::vector<HistoryPoint>& h) {
  if (h.size() < 2) return "";
  const HistoryPoint& last = h.back();
  const HistoryPoint* base = nullptr;
  for (size_t k = h.size() - 1; k-- > 0;) {
    Millis dt = last.t - h[k].t;
    if (dt > 8 * kHour) break;
    base = &h[k];
    if (dt >= 5 * kHour) break;
  }
  if (!base) return "";
  int hrs = static_cast<int>(JsRound(static_cast<double>(last.t - base->t) / kHour));
  // The page shows centimetres; the plugin shows inches (owner's decision).
  int in = static_cast<int>(JsRound((last.v - base->v) / kInch));
  if (in == 0) return "Steady over " + std::to_string(hrs) + " h";
  return std::string(in < 0 ? "Falling " : "Rising ") + std::to_string(std::abs(in)) +
         " in over " + std::to_string(hrs) + " h";
}

}  // namespace sd
