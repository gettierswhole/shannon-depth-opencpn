// Shannon Depth: the correction panel drawn on the chart canvas.
//
// The panel is drawn once into a bitmap with ordinary wxWidgets text, then
// copied to the canvas: with wxDC in normal mode, as a GL texture in OpenGL
// mode (which also works on Android's GLES2). It is only redrawn when its
// text changes.

#ifndef SHANNONDEPTH_PANEL_H_
#define SHANNONDEPTH_PANEL_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <vector>

#include "ocpn_plugin.h"

class piDC;

struct PanelLine {
  wxString text;
  wxColour colour;
  double size = 1.0;  // relative to the base font
  bool bold = false;
};

struct PanelContent {
  std::vector<PanelLine> lines;
  wxColour border;
};

class PanelRenderer {
public:
  ~PanelRenderer();
  // Re-renders only when the content changed.
  void Update(const PanelContent& content, const wxFont& base);
  wxSize Size() const { return m_bitmap.IsOk() ? m_bitmap.GetSize() : wxSize(0, 0); }
  void DrawDC(wxDC& dc, int x, int y);
  void DrawGL(piDC& dc, int x, int y);

private:
  wxString m_key;
  wxBitmap m_bitmap;
  unsigned int m_texture = 0;
  int m_tex_w = 0, m_tex_h = 0;
  bool m_texture_dirty = true;
};

#endif  // SHANNONDEPTH_PANEL_H_
