// Unit and parity tests for the Shannon Depth calculation core.
// Build: cmake -S tests -B build-tests && cmake --build build-tests && build-tests/test_core

#include <cmath>
#include <cstdio>
#include <string>

#include "../src/core/depth_core.h"
#include "parity_vectors.h"

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
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
