// Recorded input events, the macro container and pressed-state tracking.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/hotkey.h"

namespace macrolite {

enum class EventType { MouseMove, MouseButton, MouseWheel, Key };
enum class MouseButton { Left, Right, Middle, X1, X2 };

struct InputEvent {
  EventType type = EventType::MouseMove;
  int64_t timeMs = 0;  // milliseconds since the start of the recording
  int x = 0, y = 0;    // screen position (MouseMove, and informational for buttons)
  MouseButton button = MouseButton::Left;  // MouseButton
  bool down = false;                       // MouseButton and Key
  int wheelDelta = 0;                      // MouseWheel (120 = one notch)
  bool horizontalWheel = false;            // MouseWheel
  uint32_t vk = 0;                         // Key
  uint32_t scanCode = 0;                   // Key
  bool extended = false;                   // Key (extended-key flag)
};

class Macro {
 public:
  // Appends an event. Rejects (returns false, changes nothing) events with a
  // negative time or a time earlier than the previous event, so the stored
  // order is always the real order and timestamps never go backwards.
  bool append(const InputEvent& ev);

  const std::vector<InputEvent>& events() const { return events_; }
  size_t size() const { return events_.size(); }
  bool empty() const { return events_.empty(); }
  int64_t durationMs() const { return events_.empty() ? 0 : events_.back().timeMs; }
  void clear() { events_.clear(); }

  // When recording is stopped with a hotkey, the hotkey's own key presses are
  // the last things recorded. Removes key-DOWN events of the hotkey's keys that
  // were never released (i.e. were still held at the moment recording stopped).
  // Returns the number of removed events.
  size_t removeHeldHotkeyKeys(const Hotkey& hk);

 private:
  std::vector<InputEvent> events_;
};

// Tracks which keys / mouse buttons are currently held down by a stream of events.
class PressedTracker {
 public:
  void apply(const InputEvent& ev);
  bool isDown(const InputEvent& ev) const;  // key or button of `ev` currently held?
  bool anyDown() const { return !keys_.empty() || !buttons_.empty(); }
  void clear();

  // Events that release everything still held (most recently pressed first).
  std::vector<InputEvent> releaseEvents() const;

 private:
  struct DownKey {
    uint32_t vk;
    uint32_t scanCode;
    bool extended;
  };
  std::vector<DownKey> keys_;
  std::vector<MouseButton> buttons_;
};

}  // namespace macrolite
