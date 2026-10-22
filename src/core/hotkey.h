// Hotkey description, naming and validation (no Windows dependencies).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace macrolite {

// Modifier flags. The numeric values intentionally match the Windows
// MOD_ALT / MOD_CONTROL / MOD_SHIFT / MOD_WIN constants used by RegisterHotKey.
namespace mod {
constexpr uint32_t Alt = 0x1;
constexpr uint32_t Ctrl = 0x2;
constexpr uint32_t Shift = 0x4;
constexpr uint32_t Win = 0x8;
constexpr uint32_t All = 0xF;
}  // namespace mod

struct Hotkey {
  uint32_t modifiers = 0;
  uint32_t vk = 0;  // Windows virtual-key code of the non-modifier key

  bool operator==(const Hotkey& o) const { return modifiers == o.modifiers && vk == o.vk; }
  bool operator!=(const Hotkey& o) const { return !(*this == o); }
};

struct ValidationResult {
  bool ok = true;
  std::string message;  // human readable reason when !ok
};

struct NamedHotkey {
  std::string name;
  Hotkey hotkey;
};

// True for Shift/Ctrl/Alt/Win virtual keys (generic and left/right variants).
bool isModifierKey(uint32_t vk);

// True if a recorded key event with this vk is one of the keys that make up `hk`.
bool keyBelongsToHotkey(uint32_t vk, const Hotkey& hk);

// "R", "F5", "Page Up", ... Falls back to "Key 0xNN" for unknown codes.
std::string keyName(uint32_t vk);

// "Ctrl + Shift + R"
std::string toString(const Hotkey& hk);

// Checks that the combination is usable as a global hotkey.
ValidationResult validateHotkey(const Hotkey& hk);

// Checks that `candidate` is not identical to any hotkey in `others`.
ValidationResult checkConflict(const Hotkey& candidate, const std::vector<NamedHotkey>& others);

}  // namespace macrolite
