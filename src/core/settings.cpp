#include "core/settings.h"

#include <cctype>
#include <cstdint>
#include <sstream>

#include "core/repeat.h"

namespace macrolite {
namespace {

std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool parseUInt(const std::string& s, uint32_t& out) {
  if (s.empty() || s.size() > 9) return false;
  uint32_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<uint32_t>(c - '0');
  }
  out = v;
  return true;
}

bool parseHotkey(const std::string& s, Hotkey& out) {
  size_t comma = s.find(',');
  if (comma == std::string::npos) return false;
  Hotkey hk;
  if (!parseUInt(trim(s.substr(0, comma)), hk.modifiers)) return false;
  if (!parseUInt(trim(s.substr(comma + 1)), hk.vk)) return false;
  if (!validateHotkey(hk).ok) return false;
  out = hk;
  return true;
}

std::string hotkeyLine(const char* key, const Hotkey& hk) {
  return std::string(key) + "=" + std::to_string(hk.modifiers) + "," + std::to_string(hk.vk) + "\n";
}

}  // namespace

std::string serializeSettings(const Settings& s) {
  std::string out;
  out += hotkeyLine("record_hotkey", s.record);
  out += hotkeyLine("playback_hotkey", s.playback);
  out += hotkeyLine("stop_hotkey", s.stopPlayback);
  out += "repeat_count=" + std::to_string(s.repeatCount) + "\n";
  out += std::string("unlimited=") + (s.unlimited ? "1" : "0") + "\n";
  return out;
}

Settings parseSettings(const std::string& text) {
  Settings s;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = trim(line.substr(0, eq));
    std::string value = trim(line.substr(eq + 1));
    if (key == "record_hotkey") parseHotkey(value, s.record);
    else if (key == "playback_hotkey") parseHotkey(value, s.playback);
    else if (key == "stop_hotkey") parseHotkey(value, s.stopPlayback);
    else if (key == "repeat_count") {
      RepeatParseResult r = parseRepeatCount(value);
      if (r.ok) s.repeatCount = r.value;
    } else if (key == "unlimited") {
      s.unlimited = (value == "1");
    }
  }
  if (s.record == s.playback || s.record == s.stopPlayback || s.playback == s.stopPlayback) {
    Settings defaults;
    s.record = defaults.record;
    s.playback = defaults.playback;
    s.stopPlayback = defaults.stopPlayback;
  }
  return s;
}

}  // namespace macrolite
