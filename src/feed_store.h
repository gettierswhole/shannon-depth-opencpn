// Shannon Depth: fetching and caching the public feed.
//
// At start-up the last good copies are loaded from the device (datums.json
// falls back to the copy installed with the plugin). Then live.json and
// datums.json are fetched over HTTPS now and every 15 minutes. A download
// that fails its checks is ignored and the last good copy stays in use.

#ifndef SHANNONDEPTH_FEED_STORE_H_
#define SHANNONDEPTH_FEED_STORE_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/timer.h>

#include <functional>
#include <string>

#include "core/feed.h"
#include "ocpn_plugin.h"

class FeedStore : public wxEvtHandler {
public:
  static constexpr const char* kLiveUrl = "https://leehughes.ie/live.json";
  static constexpr const char* kDatumsUrl = "https://leehughes.ie/datums.json";

  FeedStore();
  ~FeedStore() override;

  void Start(std::function<void()> on_change);
  void Stop();
  void FetchNow();

  bool HasDatums() const { return m_has_datums; }
  const sd::Datums& Datums() const { return m_datums; }
  const sd::LiveFeed& Live() const { return m_live; }
  // Wall-clock time of the last successful live.json download, 0 if none.
  sd::Millis LastFetch() const { return m_last_fetch; }
  const wxString& LastError() const { return m_last_error; }
  bool DatumsFromBundle() const { return m_datums_from_bundle; }

private:
  enum class Stage { kIdle, kLive, kDatums };

  void OnTimer(wxTimerEvent& ev);
  void OnDownload(OCPN_downloadEvent& ev);
  void Begin(Stage stage);
  void Finish(bool ok);
  void LoadCached();
  bool AcceptLive(const std::string& text, bool save);
  bool AcceptDatums(const std::string& text, bool save);
  wxString CacheDir() const;

  wxTimer m_timer;
  Stage m_stage = Stage::kIdle;
  long m_handle = 0;
  wxString m_tmp;
  std::function<void()> m_on_change;
  bool m_running = false;

  bool m_has_datums = false;
  bool m_datums_from_bundle = false;
  sd::Datums m_datums;
  sd::LiveFeed m_live;
  sd::Millis m_last_fetch = 0;
  wxString m_last_error;
};

bool ReadTextFile(const wxString& path, std::string* out);
bool WriteTextFile(const wxString& path, const std::string& text);

#endif  // SHANNONDEPTH_FEED_STORE_H_
