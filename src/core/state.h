// Application state machine: Idle <-> Recording, Idle <-> Playing.
// Direct Recording <-> Playing transitions are impossible by construction.
#pragma once
#include <atomic>

namespace macrolite {

enum class AppState { Idle, Recording, Playing };

inline const char* toString(AppState s) {
  switch (s) {
    case AppState::Idle: return "Idle";
    case AppState::Recording: return "Recording";
    case AppState::Playing: return "Playing Back";
  }
  return "?";
}

class StateMachine {
 public:
  AppState state() const { return state_.load(); }
  bool beginRecording() { return transition(AppState::Idle, AppState::Recording); }
  bool endRecording() { return transition(AppState::Recording, AppState::Idle); }
  bool beginPlayback() { return transition(AppState::Idle, AppState::Playing); }
  bool endPlayback() { return transition(AppState::Playing, AppState::Idle); }

 private:
  bool transition(AppState from, AppState to) { return state_.compare_exchange_strong(from, to); }
  std::atomic<AppState> state_{AppState::Idle};
};

}  // namespace macrolite
