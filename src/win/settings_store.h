// Loads / saves settings to %APPDATA%\Macrolite\settings.ini
#pragma once
#include "core/settings.h"

namespace macrolite {

Settings loadSettings();                  // never fails; returns defaults if needed
bool saveSettings(const Settings& s);     // false if the file could not be written

}  // namespace macrolite
