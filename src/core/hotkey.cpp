#include "core/hotkey.h"

#include <cstdio>

#include "core/keys.h"

namespace macrolite {

bool isModifierKey(uint32_t vk) {
  switch (vk) {
    case vkc::Shift: case vkc::Control: case vkc::Menu:
    case vkc::LShift: case vkc::RShift:
    case vkc::LControl: case vkc::RControl:
    case vkc::LMenu: case vkc::RMenu:
    case vkc::LWin: case vkc::RWin:
      return true;
    default:
      return false;
  }
}

bool keyBelongsToHotkey(uint32_t vk, const Hotkey& hk) {
  if (vk == hk.vk) return true;
  if ((hk.modifiers & mod::Ctrl) &&
      (vk == vkc::Control || vk == vkc::LControl || vk == vkc::RControl)) return true;
  if ((hk.modifiers & mod::Shift) &&
      (vk == vkc::Shift || vk == vkc::LShift || vk == vkc::RShift)) return true;
  if ((hk.modifiers & mod::Alt) &&
      (vk == vkc::Menu || vk == vkc::LMenu || vk == vkc::RMenu)) return true;
  if ((hk.modifiers & mod::Win) && (vk == vkc::LWin || vk == vkc::RWin)) return true;
  return false;
}

std::string keyName(uint32_t vk) {
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, static_cast<char>(vk));
  if (vk >= vkc::F1 && vk <= vkc::F24) return "F" + std::to_string(vk - vkc::F1 + 1);
  if (vk >= 0x60 && vk <= 0x69) return "Num " + std::to_string(vk - 0x60);
  switch (vk) {
    case 0x08: return "Backspace";
    case 0x09: return "Tab";
    case 0x0D: return "Enter";
    case 0x13: return "Pause";
    case 0x1B: return "Esc";
    case 0x20: return "Space";
    case 0x21: return "Page Up";
    case 0x22: return "Page Down";
    case 0x23: return "End";
    case 0x24: return "Home";
    case 0x25: return "Left";
    case 0x26: return "Up";
    case 0x27: return "Right";
    case 0x28: return "Down";
    case 0x2C: return "Print Screen";
    case 0x2D: return "Insert";
    case 0x2E: return "Delete";
    case 0x6A: return "Num *";
    case 0x6B: return "Num +";
    case 0x6D: return "Num -";
    case 0x6E: return "Num .";
    case 0x6F: return "Num /";
    case 0x90: return "Num Lock";
    case 0x91: return "Scroll Lock";
    case 0xBA: return ";";
    case 0xBB: return "=";
    case 0xBC: return ",";
    case 0xBD: return "-";
    case 0xBE: return ".";
    case 0xBF: return "/";
    case 0xC0: return "`";
    case 0xDB: return "[";
    case 0xDC: return "\\";
    case 0xDD: return "]";
    case 0xDE: return "'";
    default: break;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "Key 0x%02X", static_cast<unsigned>(vk));
  return buf;
}

std::string toString(const Hotkey& hk) {
  std::string s;
  auto add = [&s](const std::string& part) {
    if (!s.empty()) s += " + ";
    s += part;
  };
  if (hk.modifiers & mod::Ctrl) add("Ctrl");
  if (hk.modifiers & mod::Alt) add("Alt");
  if (hk.modifiers & mod::Shift) add("Shift");
  if (hk.modifiers & mod::Win) add("Win");
  add(keyName(hk.vk));
  return s;
}

namespace {

bool isFunctionKey(uint32_t vk) { return vk >= vkc::F1 && vk <= vkc::F24; }

// Keys that produce characters when typed.
bool isTypingKey(uint32_t vk) {
  return (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || vk == vkc::Space ||
         (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDE);
}

struct Reserved {
  uint32_t modifiers;
  uint32_t vk;
  const char* what;
};

const Reserved kReserved[] = {
    {mod::Alt, vkc::F4, "Alt + F4 closes windows"},
    {mod::Alt, vkc::Tab, "Alt + Tab switches windows"},
    {mod::Alt, vkc::Escape, "Alt + Esc switches windows"},
    {mod::Ctrl, vkc::Escape, "Ctrl + Esc opens the Start menu"},
    {mod::Ctrl | mod::Alt, vkc::Delete, "Ctrl + Alt + Delete is reserved by Windows"},
    {mod::Ctrl | mod::Shift, vkc::Escape, "Ctrl + Shift + Esc opens Task Manager"},
    {mod::Win, 'L', "Win + L locks the computer"},
};

ValidationResult fail(const std::string& msg) {
  ValidationResult r;
  r.ok = false;
  r.message = msg;
  return r;
}

}  // namespace

ValidationResult validateHotkey(const Hotkey& hk) {
  if (hk.vk == 0 || hk.vk > 0xFE) return fail("No valid key was pressed.");
  if (isModifierKey(hk.vk))
    return fail("Press a normal key together with the modifiers (Ctrl, Alt, Shift).");
  if (hk.modifiers & ~mod::All) return fail("Unsupported modifier combination.");
  if (hk.modifiers == 0 && !isFunctionKey(hk.vk))
    return fail("Add Ctrl, Alt or Shift. Only F1-F24 may be used alone.");
  if (hk.modifiers == mod::Shift && isTypingKey(hk.vk))
    return fail("Shift + " + keyName(hk.vk) + " would block normal typing. Add Ctrl or Alt.");
  for (const Reserved& r : kReserved) {
    if (r.modifiers == hk.modifiers && r.vk == hk.vk) return fail(std::string(r.what) + ".");
  }
  return ValidationResult{};
}

ValidationResult checkConflict(const Hotkey& candidate, const std::vector<NamedHotkey>& others) {
  for (const NamedHotkey& o : others) {
    if (o.hotkey == candidate)
      return fail(toString(candidate) + " is already used for \"" + o.name + "\".");
  }
  return ValidationResult{};
}

}  // namespace macrolite
