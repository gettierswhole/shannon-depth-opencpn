// Shannon Depth: reading the public feed files. See feed.h.

#include "feed.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>

#include "json/json.h"

namespace sd {

namespace {

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant).
long long DaysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

bool ParseJson(const std::string& text, Json::Value* root, std::string* error) {
  Json::CharReaderBuilder b;
  b["collectComments"] = false;
  std::unique_ptr<Json::CharReader> r(b.newCharReader());
  std::string errs;
  if (!r->parse(text.data(), text.data() + text.size(), root, &errs)) {
    *error = "not valid JSON: " + errs;
    return false;
  }
  if (!root->isObject()) {
    *error = "not a JSON object";
    return false;
  }
  return true;
}

bool Num(const Json::Value& v, double* out) {
  if (!v.isNumeric()) return false;
  *out = v.asDouble();
  return std::isfinite(*out);
}

std::string Str(const Json::Value& v) { return v.isString() ? v.asString() : ""; }

}  // namespace

bool ParseIsoTime(const std::string& s, Millis* out) {
  int y, mo, d, h, mi;
  double sec = 0;
  char z = 0;
  int n = std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%lf%c", &y, &mo, &d, &h, &mi, &sec, &z);
  if (n == 7 && z == 'Z') {
  } else {
    n = std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d%c", &y, &mo, &d, &h, &mi, &z);
    if (n != 6 || z != 'Z') return false;
    sec = 0;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec < 0 || sec >= 61) return false;
  long long days = DaysFromCivil(y, mo, d);
  *out = ((days * 24 + h) * 60 + mi) * 60000LL + static_cast<Millis>(std::llround(sec * 1000));
  return true;
}

bool ParseDatums(const std::string& text, Datums* out, std::string* error) {
  Json::Value root;
  if (!ParseJson(text, &root, error)) return false;
  if (!root["schema"].isInt() || root["schema"].asInt() != kDatumsSchema) {
    *error = "datums.json schema is not " + std::to_string(kDatumsSchema);
    return false;
  }
  const Json::Value& list = root["stretches"];
  if (!list.isArray() || list.empty()) {
    *error = "no stretches";
    return false;
  }
  Datums d;
  d.notice = Str(root["notice"]);
  for (const auto& s : list) {
    StretchInfo si;
    Stretch& c = si.stretch;
    c.id = Str(s["id"]);
    c.name = Str(s["name"]);
    si.span = Str(s["span"]);
    c.gauge_doc = Str(s["gauge"]["doc"]);
    c.gauge_name = Str(s["gauge"]["name"]);
    c.station = Str(s["gauge"]["station"]);
    if (c.id.empty() || c.name.empty() || c.gauge_doc.empty()) {
      *error = "a stretch lacks its id, name or gauge";
      return false;
    }
    if (!Num(s["band"], &c.band) || c.band <= 0 || c.band > 1) {
      *error = c.id + ": bad band";
      return false;
    }
    const Json::Value& dt = s["datum"];
    if (dt.isObject()) {
      if (!Num(dt["level"], &c.datum) || c.datum < 20 || c.datum > 60) {
        *error = c.id + ": bad datum level";
        return false;
      }
      c.has_datum = true;
      si.datum_how = Str(dt["how"]);
    } else if (!dt.isNull()) {
      *error = c.id + ": bad datum";
      return false;
    }
    c.estimated = s["estimated"].asBool();
    si.lake = s["lake"].asBool();
    si.band_why = Str(s["bandWhy"]);
    for (const auto& cv : s["caveats"])
      if (cv.isString()) c.caveats.push_back(cv.asString());
    for (const auto& a : s["alternatives"]) {
      Alternative alt;
      alt.label = Str(a["label"]);
      if (!Num(a["level"], &alt.level) || alt.level < 20 || alt.level > 60) {
        *error = c.id + ": bad alternative datum";
        return false;
      }
      si.alternatives.push_back(alt);
    }
    double zero;
    if (Num(s["gauge"]["zero"], &zero) && zero > 20 && zero < 60) si.gauge_zero = zero;
    for (const auto& cl : s["cills"]) {
      Alternative a;
      a.label = Str(cl["name"]);
      if (Num(cl["level"], &a.level) && a.level > 20 && a.level < 60) si.cills.push_back(a);
    }
    for (const auto& p : s["boundary"]) {
      double lat, lon;
      if (!p.isArray() || p.size() != 2 || !Num(p[0], &lat) || !Num(p[1], &lon)) {
        *error = c.id + ": bad boundary point";
        return false;
      }
      c.boundary.push_back({lat, lon});
    }
    d.stretches.push_back(si);
  }
  for (const auto& z : root["lockZones"]) {
    LockZone lz;
    lz.name = Str(z["name"]);
    if (!Num(z["lat"], &lz.centre.lat) || !Num(z["lon"], &lz.centre.lon) ||
        !Num(z["r"], &lz.radius_m) || lz.radius_m <= 0 || lz.radius_m > 5000) {
      *error = "bad lock zone";
      return false;
    }
    d.zones.push_back(lz);
  }
  *out = d;
  return true;
}

bool ParseLive(const std::string& text, LiveFeed* out, std::string* error) {
  Json::Value root;
  if (!ParseJson(text, &root, error)) return false;
  const Json::Value& gauges = root["gauge"];
  if (!gauges.isObject() || gauges.empty()) {
    *error = "live.json has no gauges";
    return false;
  }
  LiveFeed f;
  for (const auto& key : gauges.getMemberNames()) {
    const Json::Value& g = gauges[key];
    GaugeFeed gf;
    gf.name = Str(g["name"]);
    double level;
    Millis t;
    if (Num(g["level"], &level) && ParseIsoTime(Str(g["time"]), &t) && level > 0 && level < 100) {
      gf.reading = Reading{level, t, true};
    }
    for (const auto& p : g["history"]) {
      double v;
      Millis pt;
      if (Num(p["v"], &v) && ParseIsoTime(Str(p["t"]), &pt)) gf.history.push_back({pt, v});
    }
    f.gauges[key] = gf;
  }
  *out = f;
  return true;
}

LiveFeed MergeLive(const LiveFeed& cached, const LiveFeed& fresh) {
  LiveFeed m = cached;
  for (const auto& kv : fresh.gauges) {
    auto it = m.gauges.find(kv.first);
    if (it == m.gauges.end() || !it->second.reading.valid ||
        (kv.second.reading.valid && kv.second.reading.time >= it->second.reading.time)) {
      m.gauges[kv.first] = kv.second;
    }
  }
  return m;
}

std::vector<Stretch> CoreStretches(const Datums& d) {
  std::vector<Stretch> v;
  for (const auto& s : d.stretches) v.push_back(s.stretch);
  return v;
}

}  // namespace sd
