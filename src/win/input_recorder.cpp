#include "win/input_recorder.h"

#include <utility>

namespace macrolite {

InputRecorder* InputRecorder::s_active = nullptr;

InputRecorder::~InputRecorder() {
  if (isRecording()) stop();
}

bool InputRecorder::start(HWND ownerWindow, std::string& error) {
  if (isRecording()) {
    error = "Already recording.";
    return false;
  }
  macro_ = Macro();
  pressed_.clear();
  truncated_ = false;
  owner_ = ownerWindow;
  startTime_ = std::chrono::steady_clock::now();
  s_active = this;

  HINSTANCE module = GetModuleHandleW(nullptr);
  mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, &InputRecorder::mouseProc, module, 0);
  if (!mouseHook_) {
    error = "Could not install the mouse hook (error " + std::to_string(GetLastError()) + ").";
    s_active = nullptr;
    return false;
  }
  keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &InputRecorder::keyboardProc, module, 0);
  if (!keyboardHook_) {
    error = "Could not install the keyboard hook (error " + std::to_string(GetLastError()) + ").";
    UnhookWindowsHookEx(mouseHook_);
    mouseHook_ = nullptr;
    s_active = nullptr;
    return false;
  }

  // Remember where the cursor starts so playback begins from the same place.
  POINT pt;
  if (GetCursorPos(&pt)) {
    InputEvent e;
    e.type = EventType::MouseMove;
    e.timeMs = 0;
    e.x = pt.x;
    e.y = pt.y;
    macro_.append(e);
  }
  return true;
}

Macro InputRecorder::stop() {
  if (keyboardHook_) UnhookWindowsHookEx(keyboardHook_);
  if (mouseHook_) UnhookWindowsHookEx(mouseHook_);
  keyboardHook_ = nullptr;
  mouseHook_ = nullptr;
  if (s_active == this) s_active = nullptr;
  owner_ = nullptr;
  return std::move(macro_);
}

int64_t InputRecorder::nowMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                               startTime_)
      .count();
}

void InputRecorder::push(InputEvent ev) {
  if (macro_.size() >= kMaxEvents) {
    truncated_ = true;
    return;
  }
  ev.timeMs = nowMs();
  macro_.append(ev);
}

bool InputRecorder::pointIsOverOwner(POINT pt) const {
  if (!owner_) return false;
  HWND under = WindowFromPoint(pt);
  return under && GetAncestor(under, GA_ROOT) == owner_;
}

LRESULT CALLBACK InputRecorder::mouseProc(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && s_active && lParam) {
    try {
      s_active->onMouse(wParam, *reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam));
    } catch (...) {
      // Never let an exception escape into Windows.
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK InputRecorder::keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION && s_active && lParam) {
    try {
      s_active->onKey(wParam, *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam));
    } catch (...) {
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

void InputRecorder::onMouse(WPARAM message, const MSLLHOOKSTRUCT& info) {
  if (info.flags & LLMHF_INJECTED) return;  // ignore synthetic input
  if (pointIsOverOwner(info.pt)) return;    // Macrolite's own window

  InputEvent e;
  e.x = info.pt.x;
  e.y = info.pt.y;

  switch (message) {
    case WM_MOUSEMOVE:
      e.type = EventType::MouseMove;
      break;
    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
      e.type = EventType::MouseButton;
      e.button = MouseButton::Left;
      e.down = (message == WM_LBUTTONDOWN);
      break;
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
      e.type = EventType::MouseButton;
      e.button = MouseButton::Right;
      e.down = (message == WM_RBUTTONDOWN);
      break;
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
      e.type = EventType::MouseButton;
      e.button = MouseButton::Middle;
      e.down = (message == WM_MBUTTONDOWN);
      break;
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
      e.type = EventType::MouseButton;
      e.button = (HIWORD(info.mouseData) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
      e.down = (message == WM_XBUTTONDOWN);
      break;
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
      e.type = EventType::MouseWheel;
      e.wheelDelta = static_cast<short>(HIWORD(info.mouseData));
      e.horizontalWheel = (message == WM_MOUSEHWHEEL);
      break;
    default:
      return;
  }

  // Drop a button release whose press we never saw (e.g. the release of the
  // click that started recording).
  if (e.type == EventType::MouseButton) {
    if (!e.down && !pressed_.isDown(e)) return;
    pressed_.apply(e);
  }
  push(e);
}

void InputRecorder::onKey(WPARAM message, const KBDLLHOOKSTRUCT& info) {
  if (info.flags & LLKHF_INJECTED) return;
  if (owner_ && GetForegroundWindow() == owner_) return;  // typing inside Macrolite

  InputEvent e;
  e.type = EventType::Key;
  switch (message) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
      e.down = true;
      break;
    case WM_KEYUP: case WM_SYSKEYUP:
      e.down = false;
      break;
    default:
      return;
  }
  e.vk = info.vkCode;
  e.scanCode = info.scanCode;
  e.extended = (info.flags & LLKHF_EXTENDED) != 0;

  // Drop a key release whose press we never saw (e.g. releasing the hotkey
  // that started recording).
  if (!e.down && !pressed_.isDown(e)) return;
  pressed_.apply(e);
  push(e);
}

}  // namespace macrolite
