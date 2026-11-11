// Unit and parity tests for the Shannon Depth calculation core.
// Build: cmake -S tests -B build-tests && cmake --build build-tests && build-tests/test_core

#include <cmath>
#include <cstdio>
#include <string>

#include "../src/core/depth_core.h"
#include "parity_vectors.h"
#include "boundary_data.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                     \
  do {                                                       \
    ++checks;                                                \
    if (!(cond)) {                                           \
      ++failures;                                            \
      std::printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
      std::printf(__VA_ARGS__);                              \
      std::printf("\n");                                     \
    }                                                        \
  } while (0)

using namespace sd;

static Stretch FromVec(const parity::StretchVec& v) {
  Stretch s;
  s.id = v.id;
  s.has_datum = v.has_datum;
  s.datum = v.datum;
  s.band = v.band;
  s.estimated = v.estimated;
  return s;
}

static const parity::StretchVec* FindVec(const std::string& id) {
  for (const auto& v : parity::kStretches)
    if (id == v.id) return &v;
  return nullptr;
}

static std::string Fixed(double x, int dp) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", dp, x);
  return buf;
}

// The page's signed(x, dp): "+0.42" or "−0.31".
static std::string Signed(double x, int dp) {
  return std::string(x < -0.0005 ? "\xE2\x88\x92" : "+") + Fixed(std::fabs(x), dp);
}

static void TestFormatting() {
  for (const auto& v : parity::kFormat) {
    CHECK(FtInText(v.n) == v.ftin, "FtInText(%d) = %s, page %s", v.n, FtInText(v.n).c_str(), v.ftin);
    CHECK(SignedFtIn(v.n) == v.signed_in, "SignedFtIn(%d) = %s, page %s", v.n, SignedFtIn(v.n).c_str(), v.signed_in);
  }
}

static void TestCorrections() {
  for (const auto& v : parity::kCorrections) {
    Stretch s = FromVec(*FindVec(v.id));
    Reading r{v.level, 0, true};
    Correction c = CorrectionOf(s, r);
    CHECK(c.available, "%s correction unavailable", v.id);
    CHECK(c.inches == v.inches, "%s level %.7f: %d in, page %d in", v.id, v.level, c.inches, v.inches);
    CHECK(c.band_inches == v.band, "%s band %d in, page %d in", v.id, c.band_inches, v.band);
  }
}

// Rebuild the page's calculator strings from the core's numbers.
static void TestCalculator() {
  const double FT = 3.28084;
  for (const auto& v : parity::kCalcs) {
    Stretch s = FromVec(*FindVec(v.id));
    Correction c = CorrectionOf(s, Reading{v.level, 0, true});
    DepthResult d = DepthAt(s, c, v.charted, v.draught);
    std::string depth = Fixed(d.depth, 2) + " m";
    std::string depth_ft = Fixed(d.depth * FT, 1) + " ft \xC2\xB7 could be " + Fixed(d.low, 2) +
                           "\xE2\x80\x93" + Fixed(d.high, 2) + " m" +
                           (d.rise_not_added ? " \xC2\xB7 high water not added"
                                             : s.estimated ? " \xC2\xB7 estimated" : "");
    std::string clear = Signed(d.clearance, 2) + " m";
    std::string clear_ft = "At worst " + Signed(d.worst, 2) + " m \xC2\xB7 " + VerdictText(d.verdict);
    CHECK(depth == v.depth, "%s %.4f/%.1f/%.1f depth '%s' page '%s'", v.id, v.level, v.charted, v.draught, depth.c_str(), v.depth);
    CHECK(depth_ft == v.depth_ft, "%s %.4f/%.1f/%.1f '%s' page '%s'", v.id, v.level, v.charted, v.draught, depth_ft.c_str(), v.depth_ft);
    CHECK(clear == v.clear, "%s %.4f/%.1f/%.1f clear '%s' page '%s'", v.id, v.level, v.charted, v.draught, clear.c_str(), v.clear);
    CHECK(clear_ft == v.clear_ft, "%s %.4f/%.1f/%.1f '%s' page '%s'", v.id, v.level, v.charted, v.draught, clear_ft.c_str(), v.clear_ft);
  }
}

static void TestFreshnessAndManual() {
  const Millis now = 1800000000000LL;
  auto at = [&](double hours) { return Reading{30.0, now - static_cast<Millis>(hours * kHour), true}; };
  // Acceptance: 2 h, 5 h, 13 h and 50 h old show green, amber, red, greyed out.
  CHECK(FreshnessOf(at(2), now) == Freshness::kCurrent, "2 h not current");
  CHECK(FreshnessOf(at(5), now) == Freshness::kAgeing, "5 h not ageing");
  CHECK(FreshnessOf(at(13), now) == Freshness::kOutOfDate, "13 h not out of date");
  CHECK(FreshnessOf(at(50), now) == Freshness::kUnusable, "50 h not unusable");
  CHECK(FreshnessOf(Reading{}, now) == Freshness::kNoData, "invalid not no-data");
  // Acceptance: your own reading is red at 4 h and gone at 13 h.
  ManualReading m{30.0, now - 1 * kHour, true};
  CHECK(ManualStateAt(m, now) == ManualState::kFresh, "manual 1 h not fresh");
  m.time = now - 4 * kHour;
  CHECK(ManualStateAt(m, now) == ManualState::kRed, "manual 4 h not red");
  m.time = now - 13 * kHour;
  CHECK(ManualStateAt(m, now) == ManualState::kLapsed, "manual 13 h not lapsed");
}

static void TestNoDatumNoNumber() {
  Stretch s;
  s.has_datum = false;
  CHECK(!CorrectionOf(s, Reading{30, 0, true}).available, "no datum gave a number");
  Stretch t = FromVec(parity::kStretches[0]);
  CHECK(!CorrectionOf(t, Reading{}).available, "no reading gave a number");
  CHECK(!DepthAt(t, Correction{}, 2.0, 1.0).available, "calculator without correction");
}

static void TestPolygons() {
  std::vector<Stretch> v(2);
  v[0].id = "a";
  v[0].boundary = {{53.0, -8.0}, {53.0, -7.9}, {53.1, -7.9}, {53.1, -8.0}};
  v[1].id = "b";
  v[1].boundary = {{53.1, -8.0}, {53.1, -7.9}, {53.2, -7.9}, {53.2, -8.0}};
  CHECK(StretchAt(v, {53.05, -7.95}) == &v[0], "point in a");
  CHECK(StretchAt(v, {53.15, -7.95}) == &v[1], "point in b");
  CHECK(StretchAt(v, {53.25, -7.95}) == nullptr, "point outside both");
  // Overlap: refuse to guess.
  v[1].boundary = {{53.04, -8.0}, {53.04, -7.9}, {53.2, -7.9}, {53.2, -8.0}};
  CHECK(StretchAt(v, {53.05, -7.95}) == nullptr, "overlap not refused");
}

// Known harbours and spots on the water, and places that must give no
// stretch. Inside a lock zone there is never a stretch.
static void TestBoundaries() {
  auto st = bdata::Stretches();
  auto zones = bdata::Zones();
  struct P { const char* where; double lat, lon; const char* expect; };
  const P pts[] = {
      {"Killaloe bridge", 52.8066, -8.4430, "derg"},
      {"Dromineer", 52.9250, -8.2780, "derg"},
      {"Portumna bridge", 53.0880, -8.2190, "derg"},
      {"Banagher bridge", 53.1920, -7.9870, "mid"},
      {"Shannonbridge", 53.2780, -8.0510, "mid"},
      {"Clonmacnoise", 53.3260, -7.9870, "mid"},
      {"Athlone town quay", 53.4255, -7.9440, "ree"},
      {"Hodson Bay", 53.4680, -8.0030, "ree"},
      {"Lanesborough", 53.6720, -7.9910, "ree"},
      {"Carrick-on-Shannon", 53.9460, -8.0920, "key"},
      {"Leitrim", 53.9905, -8.0650, "key"},
      {"Lough Key", 54.0060, -8.2350, ""},
      {"Roosky", 53.8320, -7.9200, ""},
      {"Above Tarmonbarry", 53.7470, -7.9200, ""},
      {"Dublin", 53.3500, -6.2600, ""},
      {"Victoria Lock", 53.1687, -8.0770, "lock"},
      {"Athlone Lock", 53.4219, -7.9407, "lock"},
      {"Albert Lock", 53.9106, -8.0014, "lock"},
  };
  for (const auto& p : pts) {
    Location loc = Locate(st, zones, {p.lat, p.lon});
    std::string got = loc.lock ? "lock" : loc.stretch ? loc.stretch->id : "";
    CHECK(got == p.expect, "%s: got '%s', expected '%s'", p.where, got.c_str(), p.expect);
  }
  // No two stretches overlap anywhere on a 300 m grid over the whole river.
  int overlaps = 0;
  for (double lat = 52.75; lat <= 54.05; lat += 0.0027)
    for (double lon = -8.62; lon <= -7.75; lon += 0.0045) {
      int n = 0;
      for (const auto& s : st) n += InsidePolygon(s.boundary, {lat, lon});
      overlaps += n > 1;
    }
  CHECK(overlaps == 0, "%d grid points in more than one stretch", overlaps);
  CHECK(std::fabs(DistanceMetres({53.0, -8.0}, {53.01, -8.0}) - 1112) < 2, "distance");
}

static void TestTrend() {
  std::vector<HistoryPoint> h;
  for (int i = 0; i <= 8; i++) h.push_back({i * kHour, 30.0 + i * 0.01});
  CHECK(TrendText(h) == "Rising 2 in over 5 h", "trend '%s'", TrendText(h).c_str());
  for (auto& p : h) p.v = 30.0;
  CHECK(TrendText(h) == "Steady over 5 h", "steady '%s'", TrendText(h).c_str());
  CHECK(TrendText({}).empty(), "empty trend");
}

int main() {
  TestFormatting();
  TestCorrections();
  TestCalculator();
  TestFreshnessAndManual();
  TestNoDatumNoNumber();
  TestPolygons();
  TestTrend();
  TestBoundaries();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
