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

#include "config.h"
#include "ocpn_plugin.h"

class piDC;

class ShannonDepthPi : public opencpn_plugin_118 {
public:
  explicit ShannonDepthPi(void* ppimgr);
  ~ShannonDepthPi() override = default;

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

private:
  // Draws the correction panel. Phase 1: a placeholder only.
  void DrawPanel(piDC& dc, PlugIn_ViewPort* vp);

  wxBitmap m_panel_bitmap;
};

#endif  // SHANNONDEPTH_PI_H_
