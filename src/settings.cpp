// Shannon Depth: settings and your own readings. See settings.h.

#include "settings.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/fileconf.h>

#include "ocpn_plugin.h"

namespace {
const char* kPath = "/PlugIns/ShannonDepth";
const char* kIds[] = {"derg", "mid", "ree", "key"};

wxString W(const std::string& s) { return wxString::FromUTF8(s.c_str()); }
std::string S(const wxString& s) { return std::string(s.ToUTF8()); }
}  // namespace

void Settings::Load() {
  wxFileConfig* c = GetOCPNConfigObject();
  if (!c) return;
  c->SetPath(kPath);
  c->Read("Draught", &draught, 0.0);
  long corner_l = 0;
  c->Read("PanelCorner", &corner_l, 0L);
  corner = static_cast<PanelCorner>(corner_l < 0 || corner_l > 3 ? 0 : corner_l);
  stretch = S(c->Read("Stretch", ""));
  datum_choice.clear();
  readings.clear();
  for (const char* id : kIds) {
    wxString key(id);
    long choice = -1;
    c->Read("Datum_" + key, &choice, -1L);
    if (choice >= 0) datum_choice[id] = static_cast<int>(choice);

    double level = 0, anchor = 0;
    wxString time_s;
    if (c->Read("Reading_" + key + "_Level", &level) &&
        c->Read("Reading_" + key + "_Time", &time_s)) {
      StoredReading r;
      long long t = 0;
      if (time_s.ToLongLong(&t)) {
        r.reading.level = level;
        r.reading.time = t;
        r.reading.valid = true;
        r.reading.has_anchor = c->Read("Reading_" + key + "_Anchor", &anchor);
        r.reading.anchor = anchor;
        long kind = 0;
        c->Read("Reading_" + key + "_Kind", &kind, 0L);
        r.kind = static_cast<ReadingKind>(kind < 0 || kind > 2 ? 0 : kind);
        r.detail = S(c->Read("Reading_" + key + "_Detail", ""));
        readings[id] = r;
      }
    }
  }
}

void Settings::Save() const {
  wxFileConfig* c = GetOCPNConfigObject();
  if (!c) return;
  c->SetPath(kPath);
  c->Write("Draught", draught);
  c->Write("PanelCorner", static_cast<long>(corner));
  c->Write("Stretch", W(stretch));
  for (const char* id : kIds) {
    wxString key(id);
    c->Write("Datum_" + key, static_cast<long>(DatumChoice(id)));
    auto it = readings.find(id);
    if (it == readings.end() || !it->second.reading.valid) {
      c->DeleteEntry("Reading_" + key + "_Level");
      c->DeleteEntry("Reading_" + key + "_Time");
      c->DeleteEntry("Reading_" + key + "_Anchor");
      c->DeleteEntry("Reading_" + key + "_Kind");
      c->DeleteEntry("Reading_" + key + "_Detail");
      continue;
    }
    const StoredReading& r = it->second;
    c->Write("Reading_" + key + "_Level", r.reading.level);
    c->Write("Reading_" + key + "_Time", wxString::Format("%lld", static_cast<long long>(r.reading.time)));
    if (r.reading.has_anchor)
      c->Write("Reading_" + key + "_Anchor", r.reading.anchor);
    else
      c->DeleteEntry("Reading_" + key + "_Anchor");
    c->Write("Reading_" + key + "_Kind", static_cast<long>(r.kind));
    c->Write("Reading_" + key + "_Detail", W(r.detail));
  }
  c->Flush();
}
