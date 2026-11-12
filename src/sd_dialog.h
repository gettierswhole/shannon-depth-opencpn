// Shannon Depth: the dialog opened from the toolbar button, with three
// tabs: the depth calculator, entering your own reading, and settings.

#ifndef SHANNONDEPTH_SD_DIALOG_H_
#define SHANNONDEPTH_SD_DIALOG_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/notebook.h>
#include <wx/spinctrl.h>

#include <vector>

class ShannonDepthPi;

class ShannonDepthDialog : public wxDialog {
public:
  ShannonDepthDialog(wxWindow* parent, ShannonDepthPi* pi);
  // Re-reads the plugin's state: call when data, position or settings change.
  void RefreshAll();
  void ShowTab(int tab);

private:
  wxPanel* BuildCalculator(wxNotebook* nb);
  wxPanel* BuildReading(wxNotebook* nb);
  wxPanel* BuildSettings(wxNotebook* nb);

  void UpdateCalculator();
  void UpdateReading();
  void UpdateSettings();

  void OnDraughtChanged(wxSpinEvent& ev);
  void OnUseLevel(wxCommandEvent& ev);
  void OnUseFeet(wxCommandEvent& ev);
  void OnUseSounder(wxCommandEvent& ev);
  void OnClearReading(wxCommandEvent& ev);
  void OnCornerChanged(wxCommandEvent& ev);
  void OnStretchChanged(wxCommandEvent& ev);
  void OnDatumChanged(wxCommandEvent& ev);
  void Message(const wxString& text, bool error);

  ShannonDepthPi* m_pi;
  wxNotebook* m_nb = nullptr;
  bool m_updating = false;

  // Calculator
  wxStaticText* m_calc_where = nullptr;
  wxSpinCtrl* m_chart_ft = nullptr;
  wxSpinCtrl* m_chart_in = nullptr;
  wxSpinCtrl* m_draught_ft = nullptr;
  wxSpinCtrl* m_draught_in = nullptr;
  wxStaticText* m_calc_depth = nullptr;
  wxStaticText* m_calc_range = nullptr;
  wxStaticText* m_calc_clear = nullptr;
  wxStaticText* m_calc_verdict = nullptr;
  wxStaticText* m_calc_note = nullptr;

  // Your reading
  wxStaticText* m_read_where = nullptr;
  wxStaticText* m_read_state = nullptr;
  wxButton* m_read_clear = nullptr;
  wxTextCtrl* m_level = nullptr;
  wxStaticText* m_level_help = nullptr;
  wxChoice* m_cill = nullptr;
  wxSpinCtrl* m_cill_ft = nullptr;
  wxSpinCtrl* m_cill_in = nullptr;
  wxPanel* m_cill_box = nullptr;
  wxSpinCtrl* m_snd_chart_ft = nullptr;
  wxSpinCtrl* m_snd_chart_in = nullptr;
  wxSpinCtrl* m_snd_ft = nullptr;
  wxSpinCtrl* m_snd_in = nullptr;
  wxStaticText* m_read_msg = nullptr;

  // Settings
  wxSpinCtrl* m_set_draught_ft = nullptr;
  wxSpinCtrl* m_set_draught_in = nullptr;
  wxChoice* m_corner = nullptr;
  wxChoice* m_stretch = nullptr;
  std::vector<wxString> m_stretch_ids;
  struct DatumRow {
    std::string id;
    wxChoice* choice;
  };
  std::vector<DatumRow> m_datum_rows;
  wxFlexGridSizer* m_datum_grid = nullptr;
  wxPanel* m_settings_panel = nullptr;
};

#endif  // SHANNONDEPTH_SD_DIALOG_H_
