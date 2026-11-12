// Shannon Depth: calculator, your own reading and settings. See sd_dialog.h.

#include "sd_dialog.h"

#include <cmath>
#include <string>

#include "core/depth_core.h"
#include "shannondepth_pi.h"

namespace {

const wxColour kGreen(46, 125, 50);
const wxColour kAmber(178, 106, 0);
const wxColour kRed(198, 40, 40);
const wxColour kMuted(95, 95, 95);

wxString U(const std::string& s) { return wxString::FromUTF8(s.c_str()); }

double FtIn(const wxSpinCtrl* ft, const wxSpinCtrl* in) {
  return ft->GetValue() * sd::kFoot + in->GetValue() * sd::kInch;
}

void SetFtIn(wxSpinCtrl* ft, wxSpinCtrl* in, double metres) {
  int inches = sd::InchesNear(metres);
  if (inches < 0) inches = 0;
  ft->SetValue(inches / 12);
  in->SetValue(inches % 12);
}

wxSizer* FtInRow(wxWindow* parent, wxSpinCtrl** ft, wxSpinCtrl** in, int max_ft) {
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  *ft = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                       wxSP_ARROW_KEYS, 0, max_ft, 0);
  *in = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                       wxSP_ARROW_KEYS | wxSP_WRAP, 0, 11, 0);
  row->Add(*ft, 0, wxALIGN_CENTER_VERTICAL);
  row->Add(new wxStaticText(parent, wxID_ANY, " ft  "), 0, wxALIGN_CENTER_VERTICAL);
  row->Add(*in, 0, wxALIGN_CENTER_VERTICAL);
  row->Add(new wxStaticText(parent, wxID_ANY, " in"), 0, wxALIGN_CENTER_VERTICAL);
  return row;
}

wxStaticText* Label(wxWindow* p, const wxString& t, bool bold = false, double scale = 1.0) {
  auto* l = new wxStaticText(p, wxID_ANY, t);
  wxFont f = l->GetFont();
  if (bold) f.SetWeight(wxFONTWEIGHT_BOLD);
  if (scale != 1.0) f.SetPointSize(static_cast<int>(f.GetPointSize() * scale + 0.5));
  l->SetFont(f);
  l->InvalidateBestSize();
  l->SetMinSize(l->GetBestSize());
  return l;
}

wxString WhereText(const ShannonDepthPi::State& s) {
  if (s.lock) return U("At " + s.lock->name + ": no figure near locks and weirs");
  if (!s.info) return "No stretch here. Choose one by hand in Settings if you need to.";
  wxString w = U(s.stretch.name);
  w += s.by_hand ? " (chosen by hand)" : s.from_boat ? " (where the boat is)" : " (chart centre)";
  return w;
}

}  // namespace

ShannonDepthDialog::ShannonDepthDialog(wxWindow* parent, ShannonDepthPi* pi)
    : wxDialog(parent, wxID_ANY, "Shannon Depth", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_pi(pi) {
  auto* top = new wxBoxSizer(wxVERTICAL);
  m_nb = new wxNotebook(this, wxID_ANY);
  m_nb->AddPage(BuildCalculator(m_nb), "Calculator");
  m_nb->AddPage(BuildReading(m_nb), "Your reading");
  m_nb->AddPage(BuildSettings(m_nb), "Settings");
  top->Add(m_nb, 1, wxEXPAND | wxALL, 6);
  auto* foot = Label(this, "Untested experiment, not an aid to navigation. Data: OPW, "
                           "Waterways Ireland, IWAI, CSIG.");
  foot->SetForegroundColour(kRed);
  top->Add(foot, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  auto* close = new wxButton(this, wxID_CLOSE, "Close");
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Hide(); });
  top->Add(close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 8);
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { Hide(); });
  SetSizerAndFit(top);
}

void ShannonDepthDialog::ShowTab(int tab) {
  if (tab >= 0 && tab < static_cast<int>(m_nb->GetPageCount())) m_nb->SetSelection(tab);
}

void ShannonDepthDialog::RefreshAll() {
  m_updating = true;
  UpdateCalculator();
  UpdateReading();
  UpdateSettings();
  m_updating = false;
  Layout();
  Fit();
}

// ---- Calculator ---------------------------------------------------------

wxPanel* ShannonDepthDialog::BuildCalculator(wxNotebook* nb) {
  auto* p = new wxPanel(nb);
  auto* s = new wxBoxSizer(wxVERTICAL);
  m_calc_where = Label(p, "", true);
  s->Add(m_calc_where, 0, wxALL, 8);

  auto* grid = new wxFlexGridSizer(2, 6, 10);
  grid->Add(Label(p, "Charted depth"), 0, wxALIGN_CENTER_VERTICAL);
  grid->Add(FtInRow(p, &m_chart_ft, &m_chart_in, 99));
  grid->Add(Label(p, "Your draught"), 0, wxALIGN_CENTER_VERTICAL);
  grid->Add(FtInRow(p, &m_draught_ft, &m_draught_in, 20));
  s->Add(grid, 0, wxLEFT | wxRIGHT, 8);

  m_calc_depth = Label(p, "", true, 1.3);
  m_calc_range = Label(p, "");
  m_calc_clear = Label(p, "", true);
  m_calc_verdict = Label(p, "", true, 1.3);
  m_calc_note = Label(p, "");
  m_calc_note->SetForegroundColour(kMuted);
  s->Add(m_calc_depth, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  s->Add(m_calc_range, 0, wxLEFT | wxRIGHT, 8);
  s->Add(m_calc_clear, 0, wxLEFT | wxRIGHT | wxTOP, 8);
  s->Add(m_calc_verdict, 0, wxLEFT | wxRIGHT, 8);
  s->Add(m_calc_note, 0, wxALL, 8);
  p->SetSizer(s);

  auto recalc = [this](wxSpinEvent&) {
    if (!m_updating) UpdateCalculator();
  };
  m_chart_ft->Bind(wxEVT_SPINCTRL, recalc);
  m_chart_in->Bind(wxEVT_SPINCTRL, recalc);
  m_draught_ft->Bind(wxEVT_SPINCTRL, &ShannonDepthDialog::OnDraughtChanged, this);
  m_draught_in->Bind(wxEVT_SPINCTRL, &ShannonDepthDialog::OnDraughtChanged, this);
  return p;
}

void ShannonDepthDialog::OnDraughtChanged(wxSpinEvent& ev) {
  if (m_updating) return;
  bool from_settings = ev.GetEventObject() == m_set_draught_ft ||
                       ev.GetEventObject() == m_set_draught_in;
  double d = from_settings ? FtIn(m_set_draught_ft, m_set_draught_in)
                           : FtIn(m_draught_ft, m_draught_in);
  m_pi->Prefs().draught = d;
  m_pi->SettingsChanged();
}

void ShannonDepthDialog::UpdateCalculator() {
  ShannonDepthPi::State s = m_pi->CurrentState();
  m_calc_where->SetLabel(WhereText(s));
  m_updating = true;
  SetFtIn(m_draught_ft, m_draught_in, m_pi->Prefs().draught);
  m_updating = false;

  m_calc_depth->SetLabel("");
  m_calc_range->SetLabel("");
  m_calc_clear->SetLabel("");
  m_calc_verdict->SetLabel("");
  m_calc_note->SetLabel("");
  if (!s.info || !s.usable) {
    m_calc_depth->SetLabel(s.info ? "No usable reading: use your sounder" : "");
    return;
  }
  double charted = FtIn(m_chart_ft, m_chart_in);
  if (charted <= 0) {
    m_calc_note->SetLabel("Enter the depth printed on the chart.");
    return;
  }
  sd::DepthResult d = sd::DepthAt(s.stretch, s.correction, charted, m_pi->Prefs().draught);
  if (!d.available) return;
  m_calc_depth->SetLabel(U("Water there now: about " + sd::FtInText(sd::InchesSafe(d.depth))));
  m_calc_range->SetLabel(U("Could be " + sd::FtInText(sd::InchesSafe(d.low)) + " to " +
                           sd::FtInText(sd::InchesSafe(d.high))));
  wxString note;
  if (d.rise_not_added)
    note = "River above summer level: the rise is not added.\n";
  else if (s.stretch.estimated)
    note = "Estimated: this stretch has no CSIG datum.\n";
  if (s.own) note += "Using your own reading.\n";
  note += "Judged on the shallow end. No allowance for squat or waves.";
  m_calc_note->SetLabel(note);
  if (!d.has_clearance) {
    m_calc_clear->SetLabel("Set your draught to see the clearance.");
    return;
  }
  m_calc_clear->SetLabel(U("Under the keel at worst: " + sd::SignedFtIn(sd::InchesSafe(d.worst))));
  m_calc_verdict->SetLabel(sd::VerdictText(d.verdict));
  m_calc_verdict->SetForegroundColour(d.verdict == sd::Verdict::kShouldBeFine ? kGreen
                                      : d.verdict == sd::Verdict::kGoSlow     ? kAmber
                                                                              : kRed);
}

// ---- Your reading -------------------------------------------------------

wxPanel* ShannonDepthDialog::BuildReading(wxNotebook* nb) {
  auto* p = new wxPanel(nb);
  auto* s = new wxBoxSizer(wxVERTICAL);
  m_read_where = Label(p, "", true);
  s->Add(m_read_where, 0, wxALL, 8);
  auto* state_row = new wxBoxSizer(wxHORIZONTAL);
  m_read_state = Label(p, "");
  m_read_clear = new wxButton(p, wxID_ANY, "Go back to the gauge feed");
  m_read_clear->Bind(wxEVT_BUTTON, &ShannonDepthDialog::OnClearReading, this);
  state_row->Add(m_read_state, 1, wxALIGN_CENTER_VERTICAL);
  state_row->Add(m_read_clear, 0, wxLEFT, 8);
  s->Add(state_row, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

  auto* rules = Label(p, "Your reading replaces the gauge for this stretch. It stays in use while "
                         "the official gauge stays within half the stretch's band of its level when "
                         "you entered it (up to 7 days). If the gauge moves, it turns red at 3 h and "
                         "lapses at 12 h.");
  rules->SetForegroundColour(kMuted);
  rules->Wrap(460);
  s->Add(rules, 0, wxALL, 8);

  // Gauge level or staff gauge.
  auto* b1 = new wxStaticBoxSizer(wxVERTICAL, p, "Gauge level");
  m_level_help = Label(b1->GetStaticBox(), "");
  m_level_help->SetForegroundColour(kMuted);
  b1->Add(m_level_help, 0, wxALL, 4);
  auto* r1 = new wxBoxSizer(wxHORIZONTAL);
  m_level = new wxTextCtrl(b1->GetStaticBox(), wxID_ANY, "");
  auto* use1 = new wxButton(b1->GetStaticBox(), wxID_ANY, "Use this reading");
  use1->Bind(wxEVT_BUTTON, &ShannonDepthDialog::OnUseLevel, this);
  r1->Add(m_level, 1, wxALIGN_CENTER_VERTICAL);
  r1->Add(new wxStaticText(b1->GetStaticBox(), wxID_ANY, " m  "), 0, wxALIGN_CENTER_VERTICAL);
  r1->Add(use1, 0);
  b1->Add(r1, 0, wxEXPAND | wxALL, 4);
  s->Add(b1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  // Feet over the cill.
  m_cill_box = new wxPanel(p);
  auto* b2 = new wxStaticBoxSizer(wxVERTICAL, m_cill_box, "Gauge board at a lock, feet over the cill");
  auto* r2 = new wxBoxSizer(wxHORIZONTAL);
  m_cill = new wxChoice(b2->GetStaticBox(), wxID_ANY);
  r2->Add(m_cill, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  r2->Add(FtInRow(b2->GetStaticBox(), &m_cill_ft, &m_cill_in, 16), 0, wxALIGN_CENTER_VERTICAL);
  auto* use2 = new wxButton(b2->GetStaticBox(), wxID_ANY, "Use this reading");
  use2->Bind(wxEVT_BUTTON, &ShannonDepthDialog::OnUseFeet, this);
  r2->Add(use2, 0, wxLEFT, 6);
  b2->Add(r2, 0, wxEXPAND | wxALL, 4);
  m_cill_box->SetSizer(b2);
  s->Add(m_cill_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  // Sounder check.
  auto* b3 = new wxStaticBoxSizer(wxVERTICAL, p, "Sounder or lead line at a charted spot");
  auto* h3 = Label(b3->GetStaticBox(), "Depth from the water surface: add your transducer depth. "
                                       "Take it where the chart prints a figure, on a firm bottom.");
  h3->SetForegroundColour(kMuted);
  h3->Wrap(440);
  b3->Add(h3, 0, wxALL, 4);
  auto* g3 = new wxFlexGridSizer(2, 6, 10);
  g3->Add(Label(b3->GetStaticBox(), "Charted depth there"), 0, wxALIGN_CENTER_VERTICAL);
  g3->Add(FtInRow(b3->GetStaticBox(), &m_snd_chart_ft, &m_snd_chart_in, 99));
  g3->Add(Label(b3->GetStaticBox(), "You measured"), 0, wxALIGN_CENTER_VERTICAL);
  g3->Add(FtInRow(b3->GetStaticBox(), &m_snd_ft, &m_snd_in, 99));
  b3->Add(g3, 0, wxALL, 4);
  auto* use3 = new wxButton(b3->GetStaticBox(), wxID_ANY, "Use this check");
  use3->Bind(wxEVT_BUTTON, &ShannonDepthDialog::OnUseSounder, this);
  b3->Add(use3, 0, wxALL, 4);
  s->Add(b3, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  m_read_msg = Label(p, "");
  s->Add(m_read_msg, 0, wxALL, 8);
  p->SetSizer(s);
  return p;
}

void ShannonDepthDialog::Message(const wxString& text, bool error) {
  m_read_msg->SetForegroundColour(error ? kRed : kGreen);
  m_read_msg->SetLabel(text);
  m_read_msg->Wrap(460);
  Layout();
}

void ShannonDepthDialog::UpdateReading() {
  ShannonDepthPi::State s = m_pi->CurrentState();
  m_read_where->SetLabel(WhereText(s));
  bool ok = s.info && s.stretch.has_datum;
  if (s.own) {
    std::string age = sd::AgeText(ShannonDepthPi::Now() - s.own->reading.time);
    wxString st = U("In use: your reading, " + age);
    if (s.own_state == sd::ManualState::kStable) st += ", gauge steady since";
    if (s.own_state == sd::ManualState::kRed) st += ". Check it again";
    m_read_state->SetLabel(st);
    m_read_state->SetForegroundColour(s.own_state == sd::ManualState::kRed ? kRed : kGreen);
  } else {
    m_read_state->SetLabel(ok ? "Using the gauge feed" : "");
    m_read_state->SetForegroundColour(kMuted);
  }
  m_read_clear->Enable(s.own != nullptr);

  wxString help = "m OD Malin";
  if (s.info && s.info->gauge_zero > 0)
    help = wxString::Format("m OD Malin, or the %s staff gauge (under 5 m: the gauge zero of %.3f m "
                            "is added for you)",
                            U(s.stretch.gauge_name), s.info->gauge_zero);
  m_level_help->SetLabel(help);
  m_level_help->Wrap(440);

  wxString keep = m_cill->GetStringSelection();
  m_cill->Clear();
  if (s.info)
    for (const auto& c : s.info->cills) m_cill->Append(U(c.label));
  bool has_cills = m_cill->GetCount() > 0;
  m_cill_box->Show(has_cills);
  if (has_cills) {
    int sel = keep.IsEmpty() ? wxNOT_FOUND : m_cill->FindString(keep);
    if (sel == wxNOT_FOUND) {
      // Default: the cill the stretch's datum is set over.
      sel = 0;
      if (s.stretch.id == "derg" && m_pi->Prefs().DatumChoice("derg") >= 0 && m_cill->GetCount() > 1)
        sel = 1;
    }
    m_cill->SetSelection(sel);
  }
  Layout();
}

void ShannonDepthDialog::OnUseLevel(wxCommandEvent&) {
  ShannonDepthPi::State s = m_pi->CurrentState();
  if (!s.info || !s.stretch.has_datum) {
    Message("No stretch with a datum here.", true);
    return;
  }
  double v;
  wxString txt = m_level->GetValue();
  txt.Replace(",", ".");
  if (!txt.ToCDouble(&v) || !std::isfinite(v)) {
    Message("Enter the level in metres, like 30.68 or 1.52.", true);
    return;
  }
  bool staff = v < 5 && s.info->gauge_zero > 0;
  if (staff) v += s.info->gauge_zero;
  if (s.gauge.valid && std::fabs(v - s.gauge.level) > 2.5) {
    Message(wxString::Format("That doesn't look like a reading for %s. Expect something near %.1f m.",
                             U(s.stretch.gauge_name), s.gauge.level),
            true);
    return;
  }
  std::string detail = staff ? "Staff gauge at " + s.stretch.gauge_name : "Gauge level entered by hand";
  m_pi->SetOwnReading(s.stretch.id, v, ReadingKind::kLevel, detail);
  m_level->Clear();
  Message("Your reading is in use.", false);
}

void ShannonDepthDialog::OnUseFeet(wxCommandEvent&) {
  ShannonDepthPi::State s = m_pi->CurrentState();
  int sel = m_cill->GetSelection();
  if (!s.info || !s.stretch.has_datum || sel == wxNOT_FOUND ||
      sel >= static_cast<int>(s.info->cills.size())) {
    Message("No cill heights for this stretch.", true);
    return;
  }
  double feet = m_cill_ft->GetValue() + m_cill_in->GetValue() / 12.0;
  if (feet < 2 || feet > 16) {
    Message("That doesn't look like feet over the cill. Expect about 5 to 12 ft.", true);
    return;
  }
  const sd::Alternative& cill = s.info->cills[sel];
  double v = cill.level + feet * sd::kFoot;
  std::string detail = sd::FtInText(m_cill_ft->GetValue() * 12 + m_cill_in->GetValue()) +
                       " over the " + cill.label;
  m_pi->SetOwnReading(s.stretch.id, v, ReadingKind::kFeetOverCill, detail);
  Message("Your reading is in use.", false);
}

void ShannonDepthDialog::OnUseSounder(wxCommandEvent&) {
  ShannonDepthPi::State s = m_pi->CurrentState();
  if (!s.info || !s.stretch.has_datum) {
    Message("No stretch with a datum here.", true);
    return;
  }
  double charted = FtIn(m_snd_chart_ft, m_snd_chart_in);
  double measured = FtIn(m_snd_ft, m_snd_in);
  if (charted <= 0 || measured <= 0) {
    Message("Enter both the charted depth and what you measured.", true);
    return;
  }
  double corr = measured - charted;
  if (std::fabs(corr) > 1.5) {
    Message("That is more than 5 ft from the chart. Check the spot and the transducer depth.", true);
    return;
  }
  double v = s.stretch.datum + corr;
  std::string detail = "Sounder " + sd::FtInText(sd::InchesNear(measured)) + " where charted " +
                       sd::FtInText(sd::InchesNear(charted));
  m_pi->SetOwnReading(s.stretch.id, v, ReadingKind::kSounder, detail);
  Message("Your check is in use.", false);
}

void ShannonDepthDialog::OnClearReading(wxCommandEvent&) {
  ShannonDepthPi::State s = m_pi->CurrentState();
  if (s.info) m_pi->ClearOwnReading(s.stretch.id);
  Message("Back to the gauge feed.", false);
}

// ---- Settings -----------------------------------------------------------

wxPanel* ShannonDepthDialog::BuildSettings(wxNotebook* nb) {
  auto* p = new wxPanel(nb);
  m_settings_panel = p;
  auto* s = new wxBoxSizer(wxVERTICAL);
  auto* grid = new wxFlexGridSizer(2, 8, 10);

  grid->Add(Label(p, "Your draught"), 0, wxALIGN_CENTER_VERTICAL);
  grid->Add(FtInRow(p, &m_set_draught_ft, &m_set_draught_in, 20));
  m_set_draught_ft->Bind(wxEVT_SPINCTRL, &ShannonDepthDialog::OnDraughtChanged, this);
  m_set_draught_in->Bind(wxEVT_SPINCTRL, &ShannonDepthDialog::OnDraughtChanged, this);

  grid->Add(Label(p, "Panel position"), 0, wxALIGN_CENTER_VERTICAL);
  m_corner = new wxChoice(p, wxID_ANY);
  m_corner->Append("Bottom left");
  m_corner->Append("Top left");
  m_corner->Append("Top right");
  m_corner->Append("Bottom right");
  m_corner->Bind(wxEVT_CHOICE, &ShannonDepthDialog::OnCornerChanged, this);
  grid->Add(m_corner);

  grid->Add(Label(p, "Stretch"), 0, wxALIGN_CENTER_VERTICAL);
  m_stretch = new wxChoice(p, wxID_ANY);
  m_stretch->Bind(wxEVT_CHOICE, &ShannonDepthDialog::OnStretchChanged, this);
  grid->Add(m_stretch);
  s->Add(grid, 0, wxALL, 10);

  auto* dl = Label(p, "Chart datum", true);
  s->Add(dl, 0, wxLEFT | wxRIGHT | wxTOP, 10);
  m_datum_grid = new wxFlexGridSizer(2, 6, 10);
  s->Add(m_datum_grid, 0, wxALL, 10);
  p->SetSizer(s);
  return p;
}

void ShannonDepthDialog::UpdateSettings() {
  const Settings& st = m_pi->Prefs();
  SetFtIn(m_set_draught_ft, m_set_draught_in, st.draught);
  m_corner->SetSelection(static_cast<int>(st.corner));

  const FeedStore* feed = m_pi->Feed();
  bool have = feed && feed->HasDatums();
  if (m_stretch->GetCount() == 0 && have) {
    m_stretch_ids.clear();
    m_stretch->Append("Automatic, from the boat or chart centre");
    m_stretch_ids.push_back("");
    for (const auto& si : feed->Datums().stretches) {
      m_stretch->Append(U(si.stretch.name));
      m_stretch_ids.push_back(U(si.stretch.id));
    }
  }
  for (size_t i = 0; i < m_stretch_ids.size(); i++)
    if (m_stretch_ids[i] == U(st.stretch)) m_stretch->SetSelection(static_cast<int>(i));

  if (m_datum_rows.empty() && have) {
    for (const auto& si : feed->Datums().stretches) {
      if (si.alternatives.empty()) continue;
      auto* ch = new wxChoice(m_settings_panel, wxID_ANY);
      ch->Append(U(si.datum_how.empty() ? std::string("Default") : si.datum_how) + " (default)");
      for (const auto& a : si.alternatives) ch->Append(U(a.label));
      ch->Bind(wxEVT_CHOICE, &ShannonDepthDialog::OnDatumChanged, this);
      m_datum_grid->Add(Label(m_settings_panel, U(si.stretch.name)), 0, wxALIGN_CENTER_VERTICAL);
      m_datum_grid->Add(ch);
      m_datum_rows.push_back({si.stretch.id, ch});
    }
  }
  for (auto& row : m_datum_rows) row.choice->SetSelection(st.DatumChoice(row.id) + 1);
  m_settings_panel->Layout();
}

void ShannonDepthDialog::OnCornerChanged(wxCommandEvent&) {
  if (m_updating) return;
  m_pi->Prefs().corner = static_cast<PanelCorner>(m_corner->GetSelection());
  m_pi->SettingsChanged();
}

void ShannonDepthDialog::OnStretchChanged(wxCommandEvent&) {
  if (m_updating) return;
  int i = m_stretch->GetSelection();
  if (i < 0 || i >= static_cast<int>(m_stretch_ids.size())) return;
  m_pi->Prefs().stretch = std::string(m_stretch_ids[i].ToUTF8());
  m_pi->SettingsChanged();
}

void ShannonDepthDialog::OnDatumChanged(wxCommandEvent& ev) {
  if (m_updating) return;
  for (auto& row : m_datum_rows) {
    if (row.choice != ev.GetEventObject()) continue;
    int choice = row.choice->GetSelection() - 1;
    if (row.id == "derg" && choice >= 0) {
      // Rule 7: switching Lough Derg's sill is a setting with a warning.
      int ans = wxMessageBox(
          "The lower (canal-side) Killaloe sill puts the datum about 1 ft 6 in higher, so every "
          "corrected depth on Lough Derg gets about 1 ft 6 in less water.\n\nUse it only if you "
          "have good reason to think it is the right one. It stays until you change it back.",
          "Lough Derg datum", wxOK | wxCANCEL | wxICON_WARNING, this);
      if (ans != wxOK) {
        row.choice->SetSelection(m_pi->Prefs().DatumChoice(row.id) + 1);
        return;
      }
    }
    if (choice < 0)
      m_pi->Prefs().datum_choice.erase(row.id);
    else
      m_pi->Prefs().datum_choice[row.id] = choice;
    m_pi->SettingsChanged();
  }
}
