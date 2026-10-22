#include "win/settings_store.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace macrolite {
namespace {

bool settingsPath(std::filesystem::path& out) {
  wchar_t buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return false;
  out = std::filesystem::path(buf) / L"Macrolite" / L"settings.ini";
  return true;
}

}  // namespace

Settings loadSettings() {
  try {
    std::filesystem::path p;
    if (!settingsPath(p)) return Settings();
    std::ifstream in(p, std::ios::binary);
    if (!in) return Settings();
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parseSettings(text);
  } catch (...) {
    return Settings();
  }
}

bool saveSettings(const Settings& s) {
  try {
    std::filesystem::path p;
    if (!settingsPath(p)) return false;
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << serializeSettings(s);
    return static_cast<bool>(out);
  } catch (...) {
    return false;
  }
}

}  // namespace macrolite
