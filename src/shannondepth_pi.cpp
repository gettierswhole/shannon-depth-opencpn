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

#include <wx/filename.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "pidc.h"
#include "plug_utils.h"
#include "sd_dialog.h"

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

bool InUse(sd::ManualState st) {
  return st == sd::ManualState::kFresh || st == sd::ManualState::kStable ||
         st == sd::ManualState::kRed;
}

}  // namespace

sd::Millis ShannonDepthPi::Now() { return NowMs(); }

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
  m_settings.Load();
  m_feed.reset(new FeedStore());
  m_feed->Start([this]() {
    Repaint();
    if (m_dialog) m_dialog->RefreshAll();
  });
  m_repaint_timer.Start(60 * 1000);  // the reading's age moves on

  wxString sep = wxFileName::GetPathSeparator();
  wxString dir = GetPluginDataDir(PKG_NAME) + sep + "data" + sep;
  m_tool_id = InsertPlugInToolSVG(
      "Shannon Depth", dir + "shannondepth_pi.svg", dir + "shannondepth_pi.svg",
      dir + "shannondepth_pi_toggled.svg", wxITEM_NORMAL, "Shannon Depth",
      "Depth calculator, your own reading and settings", nullptr, -1, 0, this);

  // Test hook: SHANNONDEPTH_OPEN_TAB=0|1|2 opens the dialog on that tab at
  // start-up, for automated screenshots. No effect when unset.
  wxString open_tab;
  if (wxGetEnv("SHANNONDEPTH_OPEN_TAB", &open_tab)) {
    long tab = 0;
    open_tab.ToLong(&tab);
    CallAfter([this, tab]() { ShowDialog(static_cast<int>(tab)); });
  }

  return WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK | WANTS_NMEA_EVENTS |
         INSTALLS_TOOLBAR_TOOL | WANTS_PREFERENCES;
}

bool ShannonDepthPi::DeInit() {
  if (m_dialog) {
    m_dialog->Destroy();
    m_dialog = nullptr;
  }
  if (m_tool_id >= 0) RemovePlugInTool(m_tool_id);
  m_tool_id = -1;
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

void ShannonDepthPi::OnRepaintTimer(wxTimerEvent&) {
  DropLapsedReadings();
  Repaint();
  if (m_dialog) m_dialog->RefreshAll();
}

void ShannonDepthPi::DropLapsedReadings() {
  if (!m_feed || !m_feed->HasDatums()) return;
  bool changed = false;
  for (const auto& si : m_feed->Datums().stretches) {
    auto it = m_settings.readings.find(si.stretch.id);
    if (it == m_settings.readings.end()) continue;
    sd::Reading gauge;
    std::vector<sd::HistoryPoint> hist;
    auto g = m_feed->Live().gauges.find(si.stretch.gauge_doc);
    if (g != m_feed->Live().gauges.end()) {
      gauge = g->second.reading;
      hist = g->second.history;
    }
    if (sd::ManualStateAt(it->second.reading, NowMs(), gauge, hist,
                          sd::StableTolerance(si.stretch)) == sd::ManualState::kLapsed) {
      m_settings.readings.erase(it);
      changed = true;
    }
  }
  if (changed) m_settings.Save();
}

void ShannonDepthPi::SettingsChanged() {
  m_settings.Save();
  Repaint();
  if (m_dialog) m_dialog->RefreshAll();
}

void ShannonDepthPi::SetOwnReading(const std::string& id, double level, ReadingKind kind,
                                   const std::string& detail) {
  StoredReading r;
  r.reading.level = level;
  r.reading.time = NowMs();
  r.reading.valid = true;
  r.kind = kind;
  r.detail = detail;
  // Anchor to the official gauge if it is current, so the reading can stay
  // valid while the gauge stays steady.
  if (m_feed && m_feed->HasDatums()) {
    for (const auto& si : m_feed->Datums().stretches) {
      if (si.stretch.id != id) continue;
      auto g = m_feed->Live().gauges.find(si.stretch.gauge_doc);
      if (g != m_feed->Live().gauges.end() && g->second.reading.valid &&
          NowMs() - g->second.reading.time <= sd::kStableGaugeMaxAge) {
        r.reading.has_anchor = true;
        r.reading.anchor = g->second.reading.level;
      }
    }
  }
  m_settings.readings[id] = r;
  SettingsChanged();
}

void ShannonDepthPi::ClearOwnReading(const std::string& id) {
  m_settings.readings.erase(id);
  SettingsChanged();
}

void ShannonDepthPi::OnToolbarToolCallback(int) { ShowDialog(0); }

void ShannonDepthPi::ShowPreferencesDialog(wxWindow*) { ShowDialog(2); }

void ShannonDepthPi::ShowDialog(int tab) {
  if (!m_dialog) m_dialog = new ShannonDepthDialog(GetOCPNCanvasWindow(), this);
  m_dialog->RefreshAll();
  m_dialog->ShowTab(tab);
  m_dialog->Show();
  m_dialog->Raise();
}

void ShannonDepthPi::Repaint() {
  wxWindow* canvas = GetOCPNCanvasWindow();
  if (canvas) canvas->Refresh(false);
}

ShannonDepthPi::State ShannonDepthPi::CurrentState() const {
  bool fix = NowMs() - m_fix_at <= kFixMaxAge;
  if (fix) return StateAt(m_fix_lat, m_fix_lon, true);
  return StateAt(m_view_lat, m_view_lon, false);
}

ShannonDepthPi::State ShannonDepthPi::StateAt(double lat, double lon, bool from_boat) const {
  State s;
  s.from_boat = from_boat;
  if (!m_feed || !m_feed->HasDatums()) return s;
  const sd::Datums& d = m_feed->Datums();

  if (!m_settings.stretch.empty()) {
    for (const auto& si : d.stretches)
      if (si.stretch.id == m_settings.stretch) s.info = &si;
    s.by_hand = s.info != nullptr;
  }
  if (!s.info) {
    if (!from_boat && !m_have_view) return s;
    std::vector<sd::Stretch> core = sd::CoreStretches(d);
    sd::Location loc = sd::Locate(core, d.zones, {lat, lon});
    s.lock = loc.lock;
    if (loc.stretch)
      for (const auto& si : d.stretches)
        if (si.stretch.id == loc.stretch->id) s.info = &si;
  }
  if (!s.info) return s;

  s.stretch = s.info->stretch;
  int choice = m_settings.DatumChoice(s.stretch.id);
  if (choice >= 0 && choice < static_cast<int>(s.info->alternatives.size()))
    s.stretch.datum = s.info->alternatives[choice].level;

  auto g = m_feed->Live().gauges.find(s.stretch.gauge_doc);
  if (g != m_feed->Live().gauges.end()) {
    s.gauge = g->second.reading;
    s.history = g->second.history;
  }
  sd::Millis now = NowMs();
  s.gauge_freshness = sd::FreshnessOf(s.gauge, now);
  s.gauge_correction = sd::CorrectionOf(s.stretch, s.gauge);

  auto own = m_settings.readings.find(s.stretch.id);
  if (own != m_settings.readings.end()) {
    sd::ManualState st = sd::ManualStateAt(own->second.reading, now, s.gauge, s.history,
                                           sd::StableTolerance(s.stretch));
    if (InUse(st)) {
      s.own = &own->second;
      s.own_state = st;
      s.correction = sd::CorrectionOf(
          s.stretch, sd::Reading{own->second.reading.level, own->second.reading.time, true});
      s.usable = s.correction.available;
      return s;
    }
  }
  s.correction = s.gauge_correction;
  s.usable = s.correction.available && s.gauge_freshness != sd::Freshness::kUnusable;
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
  if (s.lock) {
    add("Shannon Depth", kText, 1.0, true);
    add("No figure here", kGrey, 1.3, true);
    add(U("At " + s.lock->name), kMuted);
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

  const sd::Stretch& st = s.stretch;
  wxString where = s.by_hand ? " (chosen by hand)" : s.from_boat ? "" : " (chart centre)";
  add(U(st.name) + where, kText, 1.0, true);

  if (!s.correction.available) {
    add(st.has_datum ? "No reading yet" : "No datum for this stretch", kGrey, 1.3, true);
    add(footer, kMuted, 0.8);
    return c;
  }

  const sd::Correction& k = s.correction;
  bool own = s.own != nullptr;
  bool unusable = !own && s.gauge_freshness == sd::Freshness::kUnusable;
  wxString figure = U(sd::SignedFtIn(k.inches) + " \xC2\xB1" + sd::FtInText(k.band_inches));
  if (k.estimated) figure += " estimated";
  if (own) {
    c.border = s.own_state == sd::ManualState::kRed ? kRed : kGreen;
  } else {
    c.border = unusable ? kGrey
               : s.gauge_freshness == sd::Freshness::kCurrent ? kGreen
               : s.gauge_freshness == sd::Freshness::kAgeing ? kAmber
                                                             : kRed;
  }
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

  if (own) {
    // Your reading, and the live gauge beside it (rule 4).
    std::string age = sd::AgeText(now - s.own->reading.time);
    switch (s.own_state) {
      case sd::ManualState::kFresh: add(U("Your reading, " + age), kGreen); break;
      case sd::ManualState::kStable:
        add(U("Your reading, " + age + ", gauge steady since"), kGreen);
        break;
      default:
        add(U("Your reading, " + age + ": check it again"), kRed, 1.0, true);
        break;
    }
    if (!s.own->detail.empty()) add(U(s.own->detail), kMuted, 0.8);
    if (s.gauge_correction.available)
      add(U("Gauge now: " + sd::SignedFtIn(s.gauge_correction.inches) + ", " +
            sd::AgeText(now - s.gauge.time)),
          kMuted, 0.85);
    else
      add("Gauge now: no reading", kMuted, 0.85);
  } else {
    std::string age_text = sd::AgeText(now - s.gauge.time);
    switch (s.gauge_freshness) {
      case sd::Freshness::kCurrent: add(U("Reading " + age_text), kGreen); break;
      case sd::Freshness::kAgeing: add(U("Reading " + age_text), kAmber); break;
      case sd::Freshness::kOutOfDate:
        add(U("Out of date, " + age_text), kRed, 1.0, true);
        break;
      case sd::Freshness::kUnusable:
        add(U("Reading " + age_text + ": use your sounder"), kRed, 1.0, true);
        break;
      default: break;
    }
  }

  // Unconfirmed datum, until the CSIG settle it (rule 7).
  if (st.id == "derg") {
    int choice = m_settings.DatumChoice("derg");
    if (choice >= 0) {
      add("Using the lower Killaloe sill (your setting)", kAmber, 0.85);
    } else {
      for (const auto& a : s.info->alternatives) {
        int diff = sd::InchesNear(a.level - s.info->stretch.datum);
        if (diff > 0)
          add(U("Datum unconfirmed: the other Killaloe sill gives " + sd::FtInText(diff) +
                " less"),
              kAmber, 0.85);
      }
    }
  }

  // Gauge and trend.
  if (s.gauge.valid) {
    wxString gauge = U(st.gauge_name) + wxString::Format(" gauge %.3f m", s.gauge.level);
    std::string tr = sd::TrendText(s.history);
    if (!tr.empty()) gauge += U(" \xC2\xB7 " + tr);
    add(gauge, kMuted, 0.8);
  }
  add(footer, kMuted, 0.8);
  return c;
}

void ShannonDepthPi::Draw(PlugIn_ViewPort* vp, wxDC* dc, wxGLContext* gl) {
  m_view_lat = vp->clat;
  m_view_lon = vp->clon;
  m_have_view = true;
  State s = CurrentState();

  wxFont* f = GetOCPNScaledFont_PlugIn("Dialog");
  wxFont base = f ? *f : *wxNORMAL_FONT;
  m_renderer.Update(BuildContent(s, NowMs()), base);

  wxSize sz = m_renderer.Size();
  int margin = std::max(8, base.GetPointSize());
  bool left = m_settings.corner == PanelCorner::kBottomLeft ||
              m_settings.corner == PanelCorner::kTopLeft;
  bool top = m_settings.corner == PanelCorner::kTopLeft ||
             m_settings.corner == PanelCorner::kTopRight;
  int x = left ? margin : vp->pix_width - sz.GetWidth() - margin;
  int y = top ? margin * 4  // below the chart bar and compass
              : vp->pix_height - sz.GetHeight() - margin * 5;  // above the scale bar
  x = std::max(0, x);
  y = std::max(0, y);

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
