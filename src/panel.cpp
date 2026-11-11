// Shannon Depth: the correction panel. See panel.h.

#include "panel.h"

#include <algorithm>

#include "pidc.h"

namespace {

int NextPow2(int v) {
  int p = 1;
  while (p < v) p <<= 1;
  return p;
}

}  // namespace

PanelRenderer::~PanelRenderer() {
  // The GL texture is left for OpenCPN to release with its context; deleting
  // it here could run without a current context.
}

void PanelRenderer::Update(const PanelContent& content, const wxFont& base) {
  wxString key = content.border.GetAsString(wxC2S_HTML_SYNTAX) +
                 wxString::Format("%d|", base.GetPointSize());
  for (const auto& l : content.lines)
    key += l.text + wxString::Format("|%s|%.2f|%d\n", l.colour.GetAsString(wxC2S_HTML_SYNTAX), l.size, l.bold);
  if (key == m_key && m_bitmap.IsOk()) return;
  m_key = key;

  // Measure.
  wxBitmap probe(8, 8);
  wxMemoryDC mdc(probe);
  std::vector<wxFont> fonts;
  std::vector<wxSize> sizes;
  int width = 0, height = 0;
  for (const auto& l : content.lines) {
    wxFont f = base;
    f.SetPointSize(std::max(6, static_cast<int>(base.GetPointSize() * l.size + 0.5)));
    if (l.bold) f.SetWeight(wxFONTWEIGHT_BOLD);
    mdc.SetFont(f);
    wxCoord w, h;
    mdc.GetTextExtent(l.text.IsEmpty() ? wxString(" ") : l.text, &w, &h);
    fonts.push_back(f);
    sizes.push_back(wxSize(w, h));
    width = std::max(width, static_cast<int>(w));
    height += h;
  }
  mdc.SelectObject(wxNullBitmap);
  wxCoord pad_w, pad_h;
  {
    wxMemoryDC m2(probe);
    m2.SetFont(base);
    m2.GetTextExtent("M", &pad_w, &pad_h);
    m2.SelectObject(wxNullBitmap);
  }
  const int pad = std::max(4, static_cast<int>(pad_h / 2));
  const int border = std::max(2, pad / 3);
  width += 2 * pad + 2 * border;
  height += 2 * pad + 2 * border;

  // Draw.
  m_bitmap = wxBitmap(width, height, 24);
  mdc.SelectObject(m_bitmap);
  mdc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  mdc.Clear();
  mdc.SetPen(wxPen(content.border, border));
  mdc.SetBrush(*wxTRANSPARENT_BRUSH);
  mdc.DrawRectangle(border / 2, border / 2, width - border + 1, height - border + 1);
  int y = border + pad;
  for (size_t i = 0; i < content.lines.size(); i++) {
    mdc.SetFont(fonts[i]);
    mdc.SetTextForeground(content.lines[i].colour);
    mdc.DrawText(content.lines[i].text, border + pad, y);
    y += sizes[i].GetHeight();
  }
  mdc.SelectObject(wxNullBitmap);
  m_texture_dirty = true;
}

void PanelRenderer::DrawDC(wxDC& dc, int x, int y) {
  if (m_bitmap.IsOk()) dc.DrawBitmap(m_bitmap, x, y, false);
}

void PanelRenderer::DrawGL(piDC& dc, int x, int y) {
  if (!m_bitmap.IsOk()) return;
  const int w = m_bitmap.GetWidth(), h = m_bitmap.GetHeight();
  if (m_texture_dirty || !m_texture) {
    wxImage img = m_bitmap.ConvertToImage();
    m_tex_w = NextPow2(w);
    m_tex_h = NextPow2(h);
    std::vector<unsigned char> rgba(static_cast<size_t>(m_tex_w) * m_tex_h * 4, 0);
    const unsigned char* src = img.GetData();
    for (int r = 0; r < h; r++)
      for (int c = 0; c < w; c++) {
        size_t s = (static_cast<size_t>(r) * w + c) * 3;
        size_t d = (static_cast<size_t>(r) * m_tex_w + c) * 4;
        rgba[d] = src[s];
        rgba[d + 1] = src[s + 1];
        rgba[d + 2] = src[s + 2];
        rgba[d + 3] = 255;
      }
    if (!m_texture) {
      GLuint t;
      glGenTextures(1, &t);
      m_texture = t;
    }
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_tex_w, m_tex_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());
    m_texture_dirty = false;
  }
  glBindTexture(GL_TEXTURE_2D, m_texture);
#ifndef USE_ANDROID_GLES2
  glEnable(GL_TEXTURE_2D);
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  dc.DrawTexture(wxRect(0, 0, m_tex_w, m_tex_h), w, h, 1.0f, wxPoint(x, y), 0.0f,
                 wxPoint(0, 0));
#ifndef USE_ANDROID_GLES2
  glDisable(GL_TEXTURE_2D);
#endif
  glBindTexture(GL_TEXTURE_2D, 0);
}
