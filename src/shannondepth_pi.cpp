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

ShannonDepthPi::ShannonDepthPi(void* ppimgr) : opencpn_plugin_118(ppimgr) {
  auto icon = GetPluginIcon("shannondepth_panel_icon", PKG_NAME);
  if (icon.type == IconPath::Type::Svg)
    m_panel_bitmap = LoadSvgIcon(icon.path.c_str());
  else if (icon.type == IconPath::Type::Png)
    m_panel_bitmap = LoadPngIcon(icon.path.c_str());
  else
    wxLogWarning("ShannonDepth: cannot find panel icon");
}

int ShannonDepthPi::Init() {
  AddLocaleCatalog("opencpn-ShannonDepth_pi");
  return WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK;
}

bool ShannonDepthPi::DeInit() { return true; }

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

bool ShannonDepthPi::RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp,
                                              int canvas_ix, int priority) {
  if (priority != OVERLAY_LEGACY && priority != -1) return false;
  if (canvas_ix != 0) return false;
  piDC pidc(dc);
  pidc.SetVP(vp);
  DrawPanel(pidc, vp);
  return true;
}

bool ShannonDepthPi::RenderGLOverlayMultiCanvas(wxGLContext* pcontext,
                                                PlugIn_ViewPort* vp,
                                                int canvas_ix, int priority) {
  if (priority != OVERLAY_LEGACY && priority != -1) return false;
  if (canvas_ix != 0) return false;
#ifdef USE_ANDROID_GLES2
  static bool shaders_loaded = false;
  if (!shaders_loaded) shaders_loaded = pi_loadShaders();
#endif
  piDC pidc(pcontext);
  pidc.SetVP(vp);
  DrawPanel(pidc, vp);
  return true;
}

void ShannonDepthPi::DrawPanel(piDC& dc, PlugIn_ViewPort* vp) {
  // Phase 1 placeholder: proves the plugin loads and can draw on the chart.
  // The real panel (stretch, correction, band, age, freshness) comes in
  // phase 3.
  const wxString line1 = "Shannon Depth 0.1";
  const wxString line2 = "No stretch - no data yet";
  const wxString line3 = "Untested experiment, not an aid to navigation";

  wxFont* font = GetOCPNScaledFont_PlugIn("Dialog");
  wxFont f = font ? *font : *wxNORMAL_FONT;
  dc.SetFont(f);

  wxCoord w1, h1, w2, h2, w3, h3;
  dc.GetTextExtent(line1, &w1, &h1);
  dc.GetTextExtent(line2, &w2, &h2);
  dc.GetTextExtent(line3, &w3, &h3);

  const int pad = h1 / 2;
  const int width = std::max(w1, std::max(w2, w3)) + 2 * pad;
  const int height = h1 + h2 + h3 + 2 * pad + pad / 2;
  const int x = pad * 2;
  const int y = vp->pix_height - height - pad * 6;  // bottom-left corner

  dc.SetPen(wxPen(wxColour(60, 60, 60), 1));
  dc.SetBrush(wxBrush(wxColour(255, 255, 255, 220)));
  dc.DrawRoundedRectangle(x, y, width, height, pad / 2);

  dc.SetTextForeground(wxColour(0, 0, 0));
  dc.DrawText(line1, x + pad, y + pad);
  dc.SetTextForeground(wxColour(110, 110, 110));
  dc.DrawText(line2, x + pad, y + pad + h1);
  dc.SetTextForeground(wxColour(170, 30, 30));
  dc.DrawText(line3, x + pad, y + pad + h1 + h2 + pad / 2);
}
