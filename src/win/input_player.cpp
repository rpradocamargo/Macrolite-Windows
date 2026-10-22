#include "win/input_player.h"

#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <exception>

namespace macrolite {
namespace {

constexpr int kIterationGapMs = 10;           // pause between repetitions
constexpr int kModifierWaitTimeoutMs = 3000;  // wait for hotkey modifiers to be released

// Converts a screen pixel to SendInput's 0..65535 virtual-desktop range.
LONG toAbsolute(int value, int origin, int extent) {
  if (extent <= 1) return 0;
  long long v = (static_cast<long long>(value - origin) * 65536 + (extent - 1)) / extent;
  if (v < 0) v = 0;
  if (v > 65535) v = 65535;
  return static_cast<LONG>(v);
}

bool sendOne(const InputEvent& ev, std::string& error) {
  INPUT in = {};
  switch (ev.type) {
    case EventType::MouseMove: {
      int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
      int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
      int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
      in.type = INPUT_MOUSE;
      in.mi.dx = toAbsolute(ev.x, vx, vw);
      in.mi.dy = toAbsolute(ev.y, vy, vh);
      in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
      break;
    }
    case EventType::MouseButton: {
      in.type = INPUT_MOUSE;
      switch (ev.button) {
        case MouseButton::Left:
          in.mi.dwFlags = ev.down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
          break;
        case MouseButton::Right:
          in.mi.dwFlags = ev.down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
          break;
        case MouseButton::Middle:
          in.mi.dwFlags = ev.down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
          break;
        case MouseButton::X1:
          in.mi.dwFlags = ev.down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
          in.mi.mouseData = XBUTTON1;
          break;
        case MouseButton::X2:
          in.mi.dwFlags = ev.down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
          in.mi.mouseData = XBUTTON2;
          break;
      }
      break;
    }
    case EventType::MouseWheel:
      in.type = INPUT_MOUSE;
      in.mi.dwFlags = ev.horizontalWheel ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
      in.mi.mouseData = static_cast<DWORD>(ev.wheelDelta);
      break;
    case EventType::Key:
      in.type = INPUT_KEYBOARD;
      in.ki.wVk = static_cast<WORD>(ev.vk);
      in.ki.wScan = static_cast<WORD>(ev.scanCode);
      in.ki.dwFlags = (ev.down ? 0 : KEYEVENTF_KEYUP) | (ev.extended ? KEYEVENTF_EXTENDEDKEY : 0);
      break;
  }
  SetLastError(0);
  UINT sent = SendInput(1, &in, sizeof(INPUT));
  if (sent != 1) {
    error = "Windows rejected a simulated input event (error " + std::to_string(GetLastError()) + ").";
    return false;
  }
  return true;
}

}  // namespace

InputPlayer::InputPlayer() { stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }

InputPlayer::~InputPlayer() {
  requestStop();
  join();
  if (stopEvent_) CloseHandle(stopEvent_);
}

bool InputPlayer::start(HWND notifyWindow, UINT doneMessage, std::shared_ptr<const Macro> macro,
                        int repeat, int speedPercent, std::string& error) {
  if (!stopEvent_) {
    error = "Could not create the stop event.";
    return false;
  }
  if (thread_.joinable()) {
    error = "Playback is already running.";
    return false;
  }
  if (!macro || macro->empty()) {
    error = "There is nothing to play.";
    return false;
  }
  ResetEvent(stopEvent_);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = PlaybackResult::Finished;
    error_.clear();
  }
  try {
    thread_ = std::thread(&InputPlayer::run, this, notifyWindow, doneMessage, std::move(macro), repeat, std::clamp(speedPercent, 25, 200));
  } catch (const std::exception& e) {
    error = std::string("Could not start the playback thread: ") + e.what();
    return false;
  }
  return true;
}

void InputPlayer::requestStop() {
  if (stopEvent_) SetEvent(stopEvent_);
}

void InputPlayer::join() {
  if (thread_.joinable()) thread_.join();
}

PlaybackResult InputPlayer::result() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return result_;
}

std::string InputPlayer::errorMessage() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_;
}

bool InputPlayer::sleepUntil(std::chrono::steady_clock::time_point target) {
  using namespace std::chrono;
  for (;;) {
    if (WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0) return false;
    auto now = steady_clock::now();
    if (now >= target) return true;
    long long remaining = duration_cast<milliseconds>(target - now).count();
    if (remaining >= 1) {
      DWORD wait = static_cast<DWORD>(std::min<long long>(remaining, 1000));
      if (WaitForSingleObject(stopEvent_, wait) == WAIT_OBJECT_0) return false;
    }
    // Less than 1 ms left: loop again (short spin) for accuracy.
  }
}

// If the user is still holding the hotkey's modifiers (Ctrl+Shift+P), the
// simulated clicks would become Ctrl+Shift+clicks. Wait until they let go.
bool InputPlayer::waitForModifiersReleased() {
  const int keys[] = {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN};
  int waited = 0;
  while (waited < kModifierWaitTimeoutMs) {
    bool anyHeld = false;
    for (int k : keys)
      if (GetAsyncKeyState(k) & 0x8000) anyHeld = true;
    if (!anyHeld) return true;
    if (WaitForSingleObject(stopEvent_, 15) == WAIT_OBJECT_0) return false;
    waited += 15;
  }
  return true;  // proceed anyway after the timeout
}

void InputPlayer::releasePressed(PressedTracker& pressed) {
  std::string ignored;
  for (const InputEvent& e : pressed.releaseEvents()) sendOne(e, ignored);
  pressed.clear();
}

void InputPlayer::run(HWND notifyWindow, UINT doneMessage, std::shared_ptr<const Macro> macro,
                      int repeat, int speedPercent) {
  using namespace std::chrono;
  timeBeginPeriod(1);  // 1 ms timer resolution for accurate delays

  PlaybackResult result = PlaybackResult::Finished;
  std::string error;
  PressedTracker pressed;

  try {
    if (!waitForModifiersReleased()) {
      result = PlaybackResult::Stopped;
    } else {
      const std::vector<InputEvent>& events = macro->events();
      for (int iteration = 0; repeat == 0 || iteration < repeat; ++iteration) {
        if (iteration > 0 && !sleepUntil(steady_clock::now() + milliseconds(kIterationGapMs * 100 / speedPercent))) {
          result = PlaybackResult::Stopped;
          break;
        }
        const auto start = steady_clock::now();
        bool abort = false;
        for (const InputEvent& ev : events) {
          if (!sleepUntil(start + milliseconds(static_cast<int64_t>(ev.timeMs) * 100 / speedPercent))) {
            result = PlaybackResult::Stopped;
            abort = true;
            break;
          }
          if (!sendOne(ev, error)) {
            result = PlaybackResult::Failed;
            abort = true;
            break;
          }
          pressed.apply(ev);
        }
        releasePressed(pressed);  // never leave keys/buttons stuck down
        if (abort) break;
      }
    }
  } catch (const std::exception& e) {
    result = PlaybackResult::Failed;
    error = std::string("Unexpected error during playback: ") + e.what();
  } catch (...) {
    result = PlaybackResult::Failed;
    error = "Unexpected error during playback.";
  }

  releasePressed(pressed);
  timeEndPeriod(1);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = result;
    error_ = error;
  }
  PostMessageW(notifyWindow, doneMessage, 0, 0);
}

}  // namespace macrolite
