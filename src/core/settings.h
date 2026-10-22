// Persisted settings (hotkeys + repeat) and their text format.
#pragma once
#include <string>

#include "core/hotkey.h"

namespace macrolite {

struct Settings {
  Hotkey record{mod::Ctrl | mod::Shift, 'R'};
  Hotkey playback{mod::Ctrl | mod::Shift, 'P'};
  Hotkey stopPlayback{mod::Ctrl | mod::Shift, 'S'};
  int repeatCount = 1;
  bool unlimited = false;
};

// key=value lines, e.g. "record_hotkey=6,82".
std::string serializeSettings(const Settings& s);

// Never fails: missing, malformed or invalid values fall back to defaults.
// If the three hotkeys conflict, all hotkeys fall back to the defaults.
Settings parseSettings(const std::string& text);

}  // namespace macrolite
