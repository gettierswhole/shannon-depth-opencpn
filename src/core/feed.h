// Shannon Depth: reading the public feed files.
//
//   live.json    gauge readings with a week of history (the page's feed)
//   datums.json  stretches, datums, bands, caveats, boundaries, lock zones
//
// Both are checked before use; a file that fails is rejected whole and the
// last good copy stays in use.

#ifndef SHANNONDEPTH_FEED_H_
#define SHANNONDEPTH_FEED_H_

#include <map>
#include <string>
#include <vector>

#include "depth_core.h"

namespace sd {

constexpr int kDatumsSchema = 1;

struct Alternative {
  std::string label;
  double level;  // m OD Malin
};

struct StretchInfo {
  Stretch stretch;  // the figures the core uses
  std::string span;
  std::string datum_how;
  std::string band_why;
  bool lake = false;
  std::vector<Alternative> alternatives;
};

struct Datums {
  std::vector<StretchInfo> stretches;
  std::vector<LockZone> zones;
  std::string notice;
};

struct GaugeFeed {
  std::string name;
  Reading reading;
  std::vector<HistoryPoint> history;
};

struct LiveFeed {
  std::map<std::string, GaugeFeed> gauges;  // keyed by doc id, e.g. "portumna"
};

// "2026-11-11T19:30:00Z" (seconds and fraction optional) to milliseconds.
// Returns false on anything else.
bool ParseIsoTime(const std::string& s, Millis* out);

bool ParseDatums(const std::string& text, Datums* out, std::string* error);
bool ParseLive(const std::string& text, LiveFeed* out, std::string* error);

// Keep the newer reading for each gauge: a fresh download never replaces a
// newer cached reading with an older one.
LiveFeed MergeLive(const LiveFeed& cached, const LiveFeed& fresh);

// The core stretches, for Locate().
std::vector<Stretch> CoreStretches(const Datums& d);

}  // namespace sd

#endif  // SHANNONDEPTH_FEED_H_
