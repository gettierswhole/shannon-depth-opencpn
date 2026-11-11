// Shannon Depth: fetching and caching the public feed. See feed_store.h.

#include "feed_store.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "config.h"

namespace {
const int kTimerId = 7301;
const int kRefreshMs = 15 * 60 * 1000;
}  // namespace

bool ReadTextFile(const wxString& path, std::string* out) {
  wxFile f;
  if (!wxFileExists(path) || !f.Open(path)) return false;
  wxFileOffset len = f.Length();
  if (len <= 0 || len > 8 * 1024 * 1024) return false;
  out->resize(static_cast<size_t>(len));
  return f.Read(&(*out)[0], static_cast<size_t>(len)) == len;
}

bool WriteTextFile(const wxString& path, const std::string& text) {
  wxString tmp = path + ".tmp";
  {
    wxFile f;
    if (!f.Create(tmp, true) || !f.Write(text.data(), text.size())) return false;
  }
  return wxRenameFile(tmp, path, true);
}

FeedStore::FeedStore() : m_timer(this, kTimerId) {
  Bind(wxEVT_TIMER, &FeedStore::OnTimer, this, kTimerId);
  Connect(wxEVT_DOWNLOAD_EVENT,
          (wxObjectEventFunction)(wxEventFunction)&FeedStore::OnDownload);
}

FeedStore::~FeedStore() { Stop(); }

wxString FeedStore::CacheDir() const {
  wxString dir = *GetpPrivateApplicationDataLocation();
  dir += wxFileName::GetPathSeparator();
  dir += "plugins";
  dir += wxFileName::GetPathSeparator();
  dir += "ShannonDepth";
  if (!wxDirExists(dir)) wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  return dir;
}

void FeedStore::Start(std::function<void()> on_change) {
  m_on_change = on_change;
  m_running = true;
  LoadCached();
  if (m_on_change) m_on_change();
  m_timer.Start(kRefreshMs);
  FetchNow();
}

void FeedStore::Stop() {
  m_running = false;
  m_timer.Stop();
  if (m_stage != Stage::kIdle && m_handle) OCPN_cancelDownloadFileBackground(m_handle);
  m_stage = Stage::kIdle;
}

void FeedStore::LoadCached() {
  std::string text;
  wxString sep = wxFileName::GetPathSeparator();
  if (ReadTextFile(CacheDir() + sep + "datums.json", &text) && AcceptDatums(text, false)) {
    m_datums_from_bundle = false;
  } else {
    wxString bundled = GetPluginDataDir(PKG_NAME) + sep + "data" + sep + "datums.json";
    if (ReadTextFile(bundled, &text) && AcceptDatums(text, false)) m_datums_from_bundle = true;
  }
  if (ReadTextFile(CacheDir() + sep + "live.json", &text)) AcceptLive(text, false);
}

bool FeedStore::AcceptDatums(const std::string& text, bool save) {
  sd::Datums d;
  std::string err;
  if (!sd::ParseDatums(text, &d, &err)) {
    m_last_error = "datums.json rejected: " + wxString::FromUTF8(err.c_str());
    wxLogMessage("ShannonDepth: %s", m_last_error);
    return false;
  }
  m_datums = d;
  m_has_datums = true;
  if (save) {
    WriteTextFile(CacheDir() + wxFileName::GetPathSeparator() + "datums.json", text);
    m_datums_from_bundle = false;
  }
  return true;
}

bool FeedStore::AcceptLive(const std::string& text, bool save) {
  sd::LiveFeed f;
  std::string err;
  if (!sd::ParseLive(text, &f, &err)) {
    m_last_error = "live.json rejected: " + wxString::FromUTF8(err.c_str());
    wxLogMessage("ShannonDepth: %s", m_last_error);
    return false;
  }
  // Keep the cache only if nothing in it is newer than this download.
  bool newer_cached = false;
  for (const auto& kv : m_live.gauges) {
    auto it = f.gauges.find(kv.first);
    if (kv.second.reading.valid &&
        (it == f.gauges.end() || !it->second.reading.valid ||
         it->second.reading.time < kv.second.reading.time))
      newer_cached = true;
  }
  m_live = sd::MergeLive(m_live, f);
  if (save && !newer_cached)
    WriteTextFile(CacheDir() + wxFileName::GetPathSeparator() + "live.json", text);
  return true;
}

void FeedStore::FetchNow() {
  if (!m_running || m_stage != Stage::kIdle) return;
  Begin(Stage::kLive);
}

void FeedStore::OnTimer(wxTimerEvent&) { FetchNow(); }

void FeedStore::Begin(Stage stage) {
  m_stage = stage;
  m_tmp = wxFileName::CreateTempFileName("shannondepth");
  const char* url = stage == Stage::kLive ? kLiveUrl : kDatumsUrl;
  _OCPN_DLStatus st = OCPN_downloadFileBackground(url, m_tmp, this, &m_handle);
  if (st != OCPN_DL_NO_ERROR && st != OCPN_DL_STARTED) {
    m_last_error = wxString::Format("download of %s did not start (%d)", url, static_cast<int>(st));
    Finish(false);
  }
}

void FeedStore::OnDownload(OCPN_downloadEvent& ev) {
  if (ev.getDLEventCondition() != OCPN_DL_EVENT_TYPE_END) return;
  bool ok = ev.getDLEventStatus() == OCPN_DL_NO_ERROR;
  if (!ok) m_last_error = "download failed";
  Finish(ok);
}

void FeedStore::Finish(bool ok) {
  Stage done = m_stage;
  bool changed = false;
  if (ok) {
    std::string text;
    if (ReadTextFile(m_tmp, &text)) {
      if (done == Stage::kLive && AcceptLive(text, true)) {
        m_last_fetch = wxGetUTCTimeMillis().GetValue();
        m_last_error.clear();
        changed = true;
      } else if (done == Stage::kDatums && AcceptDatums(text, true)) {
        changed = true;
      }
    }
  }
  if (!m_tmp.IsEmpty() && wxFileExists(m_tmp)) wxRemoveFile(m_tmp);
  m_tmp.clear();
  m_handle = 0;
  m_stage = Stage::kIdle;
  // live.json first; datums.json after it, whatever happened to live.json.
  if (m_running && done == Stage::kLive) Begin(Stage::kDatums);
  if (changed && m_on_change) m_on_change();
}
