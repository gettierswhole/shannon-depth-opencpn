/***************************************************************************
 * Shannon Depth plugin for OpenCPN
 *
 * Shows how far today's water level is above or below the CSIG chart
 * datum for the Shannon stretch the boat is in.
 *
 * Untested experiment, not an aid to navigation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

#ifndef SHANNONDEPTH_PI_H_
#define SHANNONDEPTH_PI_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/timer.h>

#include <memory>

#include "config.h"
#include "core/depth_core.h"
#include "feed_store.h"
#include "ocpn_plugin.h"
#include "panel.h"
#include "settings.h"

class piDC;
class ShannonDepthDialog;

class ShannonDepthPi : public opencpn_plugin_118, public wxEvtHandler {
public:
  explicit ShannonDepthPi(void* ppimgr);
  ~ShannonDepthPi() override;

  int Init() override;
  bool DeInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;
  int GetPlugInVersionPost() override;
  const char* GetPlugInVersionPre() override;
  const char* GetPlugInVersionBuild() override;

  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

  bool RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp, int canvas_ix,
                                int priority) override;
  bool RenderGLOverlayMultiCanvas(wxGLContext* pcontext, PlugIn_ViewPort* vp,
                                  int canvas_ix, int priority) override;
  void SetPositionFixEx(PlugIn_Position_Fix_Ex& pfix) override;

  int GetToolbarToolCount() override { return 1; }
  void OnToolbarToolCallback(int id) override;
  void ShowPreferencesDialog(wxWindow* parent) override;

  // Everything the panel and the dialog show, for the stretch in use.
  struct State {
    const sd::StretchInfo* info = nullptr;
    sd::Stretch stretch;            // with the chosen datum applied
    const sd::LockZone* lock = nullptr;
    bool from_boat = false;
    bool by_hand = false;           // stretch chosen in settings

    sd::Reading gauge;              // the official gauge
    sd::Freshness gauge_freshness = sd::Freshness::kNoData;
    sd::Correction gauge_correction;
    std::vector<sd::HistoryPoint> history;

    const StoredReading* own = nullptr;  // your reading, while in use
    sd::ManualState own_state = sd::ManualState::kNone;

    sd::Correction correction;      // what to use: yours if in use, else the gauge
    bool usable = false;            // a figure may be shown and used
  };
  State CurrentState() const;

  Settings& Prefs() { return m_settings; }
  const FeedStore* Feed() const { return m_feed.get(); }
  void SettingsChanged();
  // Stores your own reading for the stretch in use, with the gauge's level
  // as its anchor.
  void SetOwnReading(const std::string& stretch_id, double level, ReadingKind kind,
                     const std::string& detail);
  void ClearOwnReading(const std::string& stretch_id);
  static sd::Millis Now();

private:
  State StateAt(double lat, double lon, bool from_boat) const;
  PanelContent BuildContent(const State& s, sd::Millis now) const;
  void Draw(PlugIn_ViewPort* vp, wxDC* dc, wxGLContext* gl);
  void OnRepaintTimer(wxTimerEvent& ev);
  void Repaint();
  void DropLapsedReadings();
  void ShowDialog(int tab);

  wxBitmap m_panel_bitmap;
  std::unique_ptr<FeedStore> m_feed;
  PanelRenderer m_renderer;
  wxTimer m_repaint_timer;
  Settings m_settings;
  ShannonDepthDialog* m_dialog = nullptr;
  int m_tool_id = -1;

  double m_fix_lat = 0, m_fix_lon = 0;
  sd::Millis m_fix_at = 0;  // wall clock when the last good fix arrived
  double m_view_lat = 0, m_view_lon = 0;  // chart centre at the last redraw
  bool m_have_view = false;
};

#endif  // SHANNONDEPTH_PI_H_
