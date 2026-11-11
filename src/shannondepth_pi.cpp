/***************************************************************************
 * Shannon Depth plugin for OpenCPN
 *
 * Untested experiment, not an aid to navigation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

#include "shannondepth_pi.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "pidc.h"
#include "plug_utils.h"

#ifdef USE_ANDROID_GLES2
#include "pi_shaders.h"
#endif

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new ShannonDepthPi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

namespace {

const int kRepaintTimerId = 7302;
const sd::Millis kFixMaxAge = 30 * 1000;  // a fix older than this is ignored

const wxColour kText(20, 20, 20);
const wxColour kMuted(95, 95, 95);
const wxColour kGrey(150, 150, 150);
const wxColour kGreen(46, 125, 50);
const wxColour kAmber(178, 106, 0);
const wxColour kRed(198, 40, 40);

wxString U(const std::string& s) { return wxString::FromUTF8(s.c_str()); }

sd::Millis NowMs() { return wxGetUTCTimeMillis().GetValue(); }

}  // namespace

ShannonDepthPi::ShannonDepthPi(void* ppimgr)
    : opencpn_plugin_118(ppimgr), m_repaint_timer(this, kRepaintTimerId) {
  auto icon = GetPluginIcon("shannondepth_panel_icon", PKG_NAME);
  if (icon.type == IconPath::Type::Svg)
    m_panel_bitmap = LoadSvgIcon(icon.path.c_str());
  else if (icon.type == IconPath::Type::Png)
    m_panel_bitmap = LoadPngIcon(icon.path.c_str());
  else
    wxLogWarning("ShannonDepth: cannot find panel icon");
}

ShannonDepthPi::~ShannonDepthPi() = default;

int ShannonDepthPi::Init() {
  AddLocaleCatalog("opencpn-ShannonDepth_pi");
  Bind(wxEVT_TIMER, &ShannonDepthPi::OnRepaintTimer, this, kRepaintTimerId);
  m_feed.reset(new FeedStore());
  m_feed->Start([this]() { Repaint(); });
  m_repaint_timer.Start(60 * 1000);  // the reading's age moves on
  return WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK | WANTS_NMEA_EVENTS;
}

bool ShannonDepthPi::DeInit() {
  m_repaint_timer.Stop();
  if (m_feed) m_feed->Stop();
  m_feed.reset();
  return true;
}

int ShannonDepthPi::GetAPIVersionMajor() { return atoi(API_VERSION); }

int ShannonDepthPi::GetAPIVersionMinor() {
  std::string v(API_VERSION);
  size_t dot = v.find('.');
  return atoi(v.substr(dot + 1).c_str());
}

int ShannonDepthPi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int ShannonDepthPi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }
int ShannonDepthPi::GetPlugInVersionPatch() { return PLUGIN_VERSION_PATCH; }
int ShannonDepthPi::GetPlugInVersionPost() { return PLUGIN_VERSION_TWEAK; }
const char* ShannonDepthPi::GetPlugInVersionPre() { return PKG_PRERELEASE; }
const char* ShannonDepthPi::GetPlugInVersionBuild() { return PKG_BUILD_INFO; }

wxBitmap* ShannonDepthPi::GetPlugInBitmap() { return &m_panel_bitmap; }
wxString ShannonDepthPi::GetCommonName() { return PLUGIN_API_NAME; }
wxString ShannonDepthPi::GetShortDescription() { return PKG_SUMMARY; }
wxString ShannonDepthPi::GetLongDescription() { return PKG_DESCRIPTION; }

void ShannonDepthPi::SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) {
  if (!std::isfinite(pfix.Lat) || !std::isfinite(pfix.Lon)) return;
  if (pfix.Lat == 0 && pfix.Lon == 0) return;
  // With no GPS, OpenCPN keeps sending its last (or default) ship position,
  // stamped with the current time but with no speed and no satellites. Only
  // a fix with a speed or satellites counts as the boat.
  if (!std::isfinite(pfix.Sog) && pfix.nSats <= 0) return;
  bool was_valid = NowMs() - m_fix_at <= kFixMaxAge;
  m_fix_lat = pfix.Lat;
  m_fix_lon = pfix.Lon;
  m_fix_at = NowMs();
  if (!was_valid) Repaint();
}

void ShannonDepthPi::OnRepaintTimer(wxTimerEvent&) { Repaint(); }

void ShannonDepthPi::Repaint() {
  wxWindow* canvas = GetOCPNCanvasWindow();
  if (canvas) canvas->Refresh(false);
}

ShannonDepthPi::State ShannonDepthPi::CurrentState(double lat, double lon,
                                                   bool from_boat) const {
  State s;
  s.from_boat = from_boat;
  if (!m_feed || !m_feed->HasDatums()) return s;
  const sd::Datums& d = m_feed->Datums();
  std::vector<sd::Stretch> core = sd::CoreStretches(d);
  sd::Location loc = sd::Locate(core, d.zones, {lat, lon});
  s.location.lock = loc.lock;
  if (!loc.stretch) return s;
  for (const auto& si : d.stretches)
    if (si.stretch.id == loc.stretch->id) s.info = &si;
  // Point at the long-lived copy, not the temporary vector.
  s.location.stretch = s.info ? &s.info->stretch : nullptr;
  if (!s.info) return s;
  auto g = m_feed->Live().gauges.find(s.info->stretch.gauge_doc);
  if (g != m_feed->Live().gauges.end()) s.reading = g->second.reading;
  s.freshness = sd::FreshnessOf(s.reading, NowMs());
  s.correction = sd::CorrectionOf(s.info->stretch, s.reading);
  return s;
}

PanelContent ShannonDepthPi::BuildContent(const State& s, sd::Millis now) const {
  PanelContent c;
  c.border = kMuted;
  auto add = [&c](const wxString& t, const wxColour& col, double size = 1.0, bool bold = false) {
    PanelLine l;
    l.text = t;
    l.colour = col;
    l.size = size;
    l.bold = bold;
    c.lines.push_back(l);
  };
  const wxString footer = "Not an aid to navigation";

  if (!m_feed || !m_feed->HasDatums()) {
    add("Shannon Depth", kText, 1.0, true);
    add("No data yet", kGrey, 1.3, true);
    add(footer, kMuted, 0.8);
    return c;
  }
  if (s.location.lock) {
    add("Shannon Depth", kText, 1.0, true);
    add("No figure here", kGrey, 1.3, true);
    add(U("At " + s.location.lock->name), kMuted);
    add("Levels change across locks and weirs", kMuted, 0.85);
    add(footer, kMuted, 0.8);
    return c;
  }
  if (!s.info) {
    add("Shannon Depth", kText, 1.0, true);
    add("No stretch", kGrey, 1.3, true);
    add(s.from_boat ? "The boat is outside the four stretches"
                    : "The chart centre is outside the four stretches",
        kMuted, 0.85);
    add(footer, kMuted, 0.8);
    return c;
  }

  const sd::Stretch& st = s.info->stretch;
  add(U(st.name) + (s.from_boat ? "" : " (chart centre)"), kText, 1.0, true);

  if (!s.correction.available) {
    add(st.has_datum ? "No reading yet" : "No datum for this stretch", kGrey, 1.3, true);
    add(footer, kMuted, 0.8);
    return c;
  }

  const sd::Correction& k = s.correction;
  sd::Millis age = now - s.reading.time;
  bool unusable = s.freshness == sd::Freshness::kUnusable;
  wxString figure = U(sd::SignedFtIn(k.inches) + " \xC2\xB1" + sd::FtInText(k.band_inches));
  if (k.estimated) figure += " estimated";
  c.border = unusable ? kGrey
             : s.freshness == sd::Freshness::kCurrent ? kGreen
             : s.freshness == sd::Freshness::kAgeing ? kAmber
                                                      : kRed;
  add(figure, unusable ? kGrey : kText, 1.6, true);

  // What to do with it.
  if (!unusable) {
    if (k.estimated) {
      if (k.metres > 0.005)
        add("River above summer level: not added to charted depths", kMuted, 0.85);
      else if (k.inches < 0)
        add(U("Expect about " + sd::FtInText(k.inches) + " less than charted"), kMuted, 0.85);
      else
        add("River at summer level", kMuted, 0.85);
    } else {
      if (k.inches > 0)
        add(U("Add " + sd::FtInText(k.inches) + " to charted depths"), kMuted, 0.85);
      else if (k.inches < 0)
        add(U("Take " + sd::FtInText(k.inches) + " off charted depths"), kMuted, 0.85);
      else
        add("Charted depths are what you have", kMuted, 0.85);
    }
  }

  // Freshness.
  std::string age_text = sd::AgeText(age);
  switch (s.freshness) {
    case sd::Freshness::kCurrent: add(U("Reading " + age_text), kGreen); break;
    case sd::Freshness::kAgeing: add(U("Reading " + age_text), kAmber); break;
    case sd::Freshness::kOutOfDate: add(U("Out of date, " + age_text), kRed, 1.0, true); break;
    case sd::Freshness::kUnusable:
      add(U("Reading " + age_text + ": use your sounder"), kRed, 1.0, true);
      break;
    default: break;
  }

  // Unconfirmed datum, until the CSIG settle it (rule 7).
  if (st.id == "derg") {
    for (const auto& a : s.info->alternatives) {
      int diff = sd::InchesNear(a.level - st.datum);
      if (diff > 0)
        add(U("Datum unconfirmed: the other Killaloe sill gives " + sd::FtInText(diff) + " less"),
            kAmber, 0.85);
    }
  }

  // Gauge and trend.
  wxString gauge = U(st.gauge_name) + wxString::Format(" gauge %.3f m", s.reading.level);
  auto g = m_feed->Live().gauges.find(st.gauge_doc);
  if (g != m_feed->Live().gauges.end()) {
    std::string tr = sd::TrendText(g->second.history);
    if (!tr.empty()) gauge += U(" \xC2\xB7 " + tr);
  }
  add(gauge, kMuted, 0.8);
  add(footer, kMuted, 0.8);
  return c;
}

void ShannonDepthPi::Draw(PlugIn_ViewPort* vp, wxDC* dc, wxGLContext* gl) {
  bool fix = NowMs() - m_fix_at <= kFixMaxAge;
  double lat = fix ? m_fix_lat : vp->clat;
  double lon = fix ? m_fix_lon : vp->clon;
  State s = CurrentState(lat, lon, fix);

  wxFont* f = GetOCPNScaledFont_PlugIn("Dialog");
  wxFont base = f ? *f : *wxNORMAL_FONT;
  m_renderer.Update(BuildContent(s, NowMs()), base);

  wxSize sz = m_renderer.Size();
  int margin = std::max(8, base.GetPointSize());
  int x = margin;
  int y = vp->pix_height - sz.GetHeight() - margin * 5;  // above the scale bar
  if (y < margin) y = margin;

  if (dc) {
    m_renderer.DrawDC(*dc, x, y);
  } else if (gl) {
#ifdef USE_ANDROID_GLES2
    static bool shaders_loaded = false;
    if (!shaders_loaded) shaders_loaded = pi_loadShaders();
#endif
    piDC pidc(gl);
    pidc.SetVP(vp);
    m_renderer.DrawGL(pidc, x, y);
  }
}

bool ShannonDepthPi::RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp,
                                              int canvas_ix, int priority) {
  if (priority != OVERLAY_LEGACY && priority != -1) return false;
  if (canvas_ix != 0) return false;
  Draw(vp, &dc, nullptr);
  return true;
}

bool ShannonDepthPi::RenderGLOverlayMultiCanvas(wxGLContext* pcontext,
                                                PlugIn_ViewPort* vp,
                                                int canvas_ix, int priority) {
  if (priority != OVERLAY_LEGACY && priority != -1) return false;
  if (canvas_ix != 0) return false;
  Draw(vp, nullptr, pcontext);
  return true;
}
