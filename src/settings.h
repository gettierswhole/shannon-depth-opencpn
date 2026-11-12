// Shannon Depth: settings and your own readings, kept in OpenCPN's config
// file under [PlugIns/ShannonDepth].

#ifndef SHANNONDEPTH_SETTINGS_H_
#define SHANNONDEPTH_SETTINGS_H_

#include <map>
#include <string>

#include "core/depth_core.h"

enum class PanelCorner { kBottomLeft = 0, kTopLeft = 1, kTopRight = 2, kBottomRight = 3 };

// How your own reading was taken, for the panel's wording.
enum class ReadingKind { kLevel = 0, kFeetOverCill = 1, kSounder = 2 };

struct StoredReading {
  sd::ManualReading reading;
  ReadingKind kind = ReadingKind::kLevel;
  std::string detail;  // "6 ft 9 in over the Athlone upper cill"
};

struct Settings {
  double draught = 0;  // metres; 0 = not set
  PanelCorner corner = PanelCorner::kBottomLeft;
  std::string stretch;  // "" = automatic, else a stretch id
  std::map<std::string, int> datum_choice;  // stretch id -> alternative, -1 = default
  std::map<std::string, StoredReading> readings;  // stretch id -> your reading

  void Load();
  void Save() const;
  int DatumChoice(const std::string& id) const {
    auto it = datum_choice.find(id);
    return it == datum_choice.end() ? -1 : it->second;
  }
};

#endif  // SHANNONDEPTH_SETTINGS_H_
