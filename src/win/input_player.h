// Plays a recorded macro on a background thread using SendInput.
#pragma once
#include <windows.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/events.h"

namespace macrolite {

enum class PlaybackResult { Finished, Stopped, Failed };

class InputPlayer {
 public:
  InputPlayer();
  ~InputPlayer();
  InputPlayer(const InputPlayer&) = delete;
  InputPlayer& operator=(const InputPlayer&) = delete;

  // Starts playback on a worker thread. `repeat` = number of repetitions,
  // 0 = until stopped. When the worker ends it posts `doneMessage` to
  // `notifyWindow`; the owner must then call join(). Returns false (and fills
  // `error`) if a worker is already active or the thread cannot be created.
  bool start(HWND notifyWindow, UINT doneMessage, std::shared_ptr<const Macro> macro, int repeat, int speedPercent, std::string& error);

  // Asks the worker to stop. Returns immediately; the worker reacts within
  // about a millisecond, even in the middle of a long delay.
  void requestStop();

  // Waits for the worker to exit (a no-op if none is running).
  void join();

  PlaybackResult result() const;
  std::string errorMessage() const;

 private:
  void run(HWND notifyWindow, UINT doneMessage, std::shared_ptr<const Macro> macro, int repeat,
           int speedPercent);
  bool sleepUntil(std::chrono::steady_clock::time_point target);  // false => stop requested
  bool waitForModifiersReleased();
  void releasePressed(PressedTracker& pressed);

  HANDLE stopEvent_ = nullptr;
  std::thread thread_;
  mutable std::mutex mutex_;
  PlaybackResult result_ = PlaybackResult::Finished;
  std::string error_;
};

}  // namespace macrolite
