// Tests for reading live.json and datums.json.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/core/feed.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                               \
  do {                                                 \
    ++checks;                                          \
    if (!(cond)) {                                     \
      ++failures;                                      \
      std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      std::printf(__VA_ARGS__);                        \
      std::printf("\n");                               \
    }                                                  \
  } while (0)

using namespace sd;

static std::string Slurp(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main(int argc, char** argv) {
  std::string root = argc > 1 ? argv[1] : "..";
  std::string err;

  Millis t;
  CHECK(ParseIsoTime("1970-01-01T00:00:00Z", &t) && t == 0, "epoch");
  CHECK(ParseIsoTime("2026-11-11T19:30:00Z", &t) && t == 1794425400000LL, "time %lld", (long long)t);
  CHECK(ParseIsoTime("2026-11-11T19:30Z", &t) && t == 1794425400000LL, "no seconds");
  CHECK(!ParseIsoTime("2026-11-11 19:30", &t), "no Z accepted");
  CHECK(!ParseIsoTime("garbage", &t), "garbage accepted");

  Datums d;
  std::string dj = Slurp(root + "/data/datums.json");
  CHECK(ParseDatums(dj, &d, &err), "bundled datums.json rejected: %s", err.c_str());
  CHECK(d.stretches.size() == 4, "%zu stretches", d.stretches.size());
  CHECK(d.zones.size() >= 10, "%zu zones", d.zones.size());
  if (d.stretches.size() == 4) {
    CHECK(d.stretches[0].stretch.id == "derg" && std::abs(d.stretches[0].stretch.datum - 30.2484) < 1e-9, "derg datum");
    CHECK(d.stretches[0].alternatives.size() == 1, "derg alternative");
    CHECK(std::abs(d.stretches[0].gauge_zero - 29.169) < 1e-9, "derg gauge zero");
    CHECK(d.stretches[0].cills.size() == 2 && d.stretches[3].cills.empty(), "cills");
    CHECK(d.stretches[1].stretch.estimated && d.stretches[3].stretch.estimated, "estimated flags");
    CHECK(!d.stretches[2].stretch.estimated && d.stretches[2].stretch.boundary.size() > 4, "ree");
  }

  // Malformed files are rejected whole.
  Datums keep = d;
  CHECK(!ParseDatums("{not json", &d, &err), "broken JSON accepted");
  CHECK(!ParseDatums("{\"schema\":2,\"stretches\":[]}", &d, &err), "schema 2 accepted");
  std::string bad = dj;
  bad.replace(bad.find("\"band\": 0.1"), 11, "\"band\": \"x\"");
  CHECK(!ParseDatums(bad, &d, &err), "bad band accepted");
  CHECK(d.stretches.size() == keep.stretches.size(), "failed parse changed the output");

  LiveFeed live;
  CHECK(ParseLive(Slurp(root + "/tests/fixtures/live.json"), &live, &err), "live.json rejected: %s", err.c_str());
  for (const char* g : {"portumna", "banagher", "athloneus", "carrick"}) {
    auto it = live.gauges.find(g);
    CHECK(it != live.gauges.end() && it->second.reading.valid && it->second.history.size() > 100, "gauge %s", g);
  }
  CHECK(!ParseLive("{\"gauge\":{}}", &live, &err), "empty gauges accepted");

  // An older download never replaces a newer cached reading.
  LiveFeed a, b;
  a.gauges["x"].reading = Reading{30.1, 2000, true};
  b.gauges["x"].reading = Reading{30.5, 1000, true};
  b.gauges["y"].reading = Reading{31.0, 1000, true};
  LiveFeed m = MergeLive(a, b);
  CHECK(m.gauges["x"].reading.level == 30.1 && m.gauges["y"].reading.level == 31.0, "merge");

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
