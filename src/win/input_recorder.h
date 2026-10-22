// Records global mouse and keyboard input using low-level hooks.
//
// Threading: WH_MOUSE_LL / WH_KEYBOARD_LL hooks are called on the thread that
// installed them, via that thread's message loop. Macrolite installs them on
// the UI thread, so the recorder's data is only ever touched by one thread and
// needs no locking. Hook callbacks never block and never swallow input.
#pragma once
#include <windows.h>

#include <chrono>
#include <string>

#include "core/events.h"

namespace macrolite {

class InputRecorder {
 public:
  InputRecorder() = default;
  ~InputRecorder();
  InputRecorder(const InputRecorder&) = delete;
  InputRecorder& operator=(const InputRecorder&) = delete;

  // Installs the hooks. `ownerWindow`'s own area is excluded from recording so
  // clicks on Macrolite's buttons are not recorded. On failure returns false
  // and fills `error`.
  bool start(HWND ownerWindow, std::string& error);

  // Removes the hooks and returns everything recorded.
  Macro stop();

  bool isRecording() const { return mouseHook_ != nullptr; }
  bool truncated() const { return truncated_; }  // event limit reached

  static constexpr size_t kMaxEvents = 500000;

 private:
  static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam);

  void onMouse(WPARAM message, const MSLLHOOKSTRUCT& info);
  void onKey(WPARAM message, const KBDLLHOOKSTRUCT& info);
  void push(InputEvent ev);
  int64_t nowMs() const;
  bool pointIsOverOwner(POINT pt) const;

  static InputRecorder* s_active;

  HHOOK mouseHook_ = nullptr;
  HHOOK keyboardHook_ = nullptr;
  HWND owner_ = nullptr;
  Macro macro_;
  PressedTracker pressed_;
  std::chrono::steady_clock::time_point startTime_;
  bool truncated_ = false;
};

}  // namespace macrolite
