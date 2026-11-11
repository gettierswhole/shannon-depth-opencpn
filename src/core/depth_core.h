// Shannon Depth: calculation core.
//
// Plain C++ with no OpenCPN or wxWidgets dependency, so it can be unit
// tested on its own. Every rule here mirrors the Shannon Chart Depth page
// (index.html); tests/parity checks the two against each other.
//
// Untested experiment, not an aid to navigation.

#ifndef SHANNONDEPTH_DEPTH_CORE_H_
#define SHANNONDEPTH_DEPTH_CORE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace sd {

constexpr double kFoot = 0.3048;
constexpr double kInch = 0.0254;

// Times are milliseconds since the Unix epoch, as in the page.
using Millis = int64_t;
constexpr Millis kHour = 3600 * 1000LL;

// ---- Imperial display ---------------------------------------------------

// Water over chart datum in whole inches, rounded toward less water:
// +0.41 m gives 16 in, -0.10 m gives -4 in. (page: inSafe)
int InchesSafe(double metres);
// Nearest whole inch. (page: inNear)
int InchesNear(double metres);
// Whole inches rounded up, sign dropped; used for bands. (page: inUp)
int InchesUp(double metres);
// "1 ft 4 in", "4 in", "0 in" from whole inches, sign dropped. (page: ftInText)
std::string FtInText(int inches);
// "+1 ft 4 in" or "−4 in" (true minus sign). (page: signedIn)
std::string SignedFtIn(int inches);

// ---- Stretches and datums -----------------------------------------------

struct LatLon {
  double lat;
  double lon;
};

struct Stretch {
  std::string id;          // "derg", "mid", "ree", "key"
  std::string name;        // "Lough Derg"
  std::string gauge_doc;   // key in live.json "gauge"
  std::string gauge_name;  // "Portumna Bridge"
  std::string station;     // OPW station number
  bool has_datum = false;  // false: no figure at all, panel shows none
  double datum = 0;        // m OD Malin
  bool estimated = false;  // against a summer level, not a chart datum
  double band = 0.10;      // +/- metres
  std::vector<std::string> caveats;
  std::vector<LatLon> boundary;  // closed polygon, may be empty
};

// True if the point is inside the polygon (ray casting; edges count as
// inside or outside arbitrarily, so boundaries should not run through
// places a boat would sit).
bool InsidePolygon(const std::vector<LatLon>& poly, LatLon p);

// The stretch containing p, or nullptr. Never guesses a neighbour.
const Stretch* StretchAt(const std::vector<Stretch>& stretches, LatLon p);

// ---- Readings -----------------------------------------------------------

struct Reading {
  double level = 0;  // m OD Malin
  Millis time = 0;
  bool valid = false;
};

struct HistoryPoint {
  Millis t;
  double v;
};

// Your own reading, entered on the device.
struct ManualReading {
  double level = 0;
  Millis time = 0;
  bool valid = false;
};

constexpr Millis kManualRed = 3 * kHour;   // turns red after 3 h
constexpr Millis kManualMax = 12 * kHour;  // lapses at 12 h

enum class ManualState { kNone, kFresh, kRed, kLapsed };
ManualState ManualStateAt(const ManualReading& m, Millis now);

// ---- Freshness ----------------------------------------------------------

enum class Freshness { kNoData, kCurrent, kAgeing, kOutOfDate, kUnusable };

struct FreshnessRules {
  Millis current = 3 * kHour;    // green below this
  Millis ageing = 12 * kHour;    // amber below this, red above
  Millis unusable = 48 * kHour;  // greyed out, use the sounder
};

Freshness FreshnessOf(const Reading& r, Millis now,
                      const FreshnessRules& rules = FreshnessRules());

// ---- Correction ---------------------------------------------------------

struct Correction {
  bool available = false;  // false: show no number
  double metres = 0;       // level - datum
  int inches = 0;          // rounded toward less water
  int band_inches = 0;     // rounded up
  bool estimated = false;
};

Correction CorrectionOf(const Stretch& s, const Reading& r);

// ---- Depth calculator ---------------------------------------------------

enum class Verdict { kNone, kShouldBeFine, kGoSlow, kTooTight, kCouldTouch };

// Thresholds on the worst case, as on the page (0.6 m is about 2 ft,
// 0.3 m about 1 ft).
constexpr double kGoSlowBelow = 0.6;
constexpr double kTooTightBelow = 0.3;

struct DepthResult {
  bool available = false;
  double depth = 0;         // best estimate of water there now, m
  double low = 0;           // shallow end of the band, never below 0
  double high = 0;          // deep end
  bool rise_not_added = false;  // estimated stretch above summer level
  bool has_clearance = false;
  double clearance = 0;     // depth - draught
  double worst = 0;         // clearance at the shallow end
  Verdict verdict = Verdict::kNone;
};

// charted and draught in metres; draught <= 0 means not set.
// On an estimated stretch a rise above summer level is not added, because
// on a river the extra water isn't spread evenly (page: calc()).
DepthResult DepthAt(const Stretch& s, const Correction& c, double charted,
                    double draught);

const char* VerdictText(Verdict v);

// ---- Trend --------------------------------------------------------------

// "Rising 3 cm over 6 h" style text from the gauge history, or empty.
// (page: trend)
std::string TrendText(const std::vector<HistoryPoint>& h);

}  // namespace sd

#endif  // SHANNONDEPTH_DEPTH_CORE_H_
