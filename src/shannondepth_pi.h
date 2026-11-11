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

class piDC;

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

  // Where the panel's figures come from, for the calculator (phase 3b).
  struct State {
    const sd::StretchInfo* info = nullptr;
    sd::Location location;
    bool from_boat = false;
    sd::Reading reading;
    sd::Correction correction;
    sd::Freshness freshness = sd::Freshness::kNoData;
  };
  State CurrentState(double lat, double lon, bool from_boat) const;

private:
  PanelContent BuildContent(const State& s, sd::Millis now) const;
  void Draw(PlugIn_ViewPort* vp, wxDC* dc, wxGLContext* gl);
  void OnRepaintTimer(wxTimerEvent& ev);
  void Repaint();

  wxBitmap m_panel_bitmap;
  std::unique_ptr<FeedStore> m_feed;
  PanelRenderer m_renderer;
  wxTimer m_repaint_timer;

  double m_fix_lat = 0, m_fix_lon = 0;
  sd::Millis m_fix_at = 0;  // wall clock when the last good fix arrived
};

#endif  // SHANNONDEPTH_PI_H_
