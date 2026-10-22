#include "core/events.h"

#include <algorithm>
#include <map>

namespace macrolite {

bool Macro::append(const InputEvent& ev) {
  if (ev.timeMs < 0) return false;
  if (!events_.empty() && ev.timeMs < events_.back().timeMs) return false;
  events_.push_back(ev);
  return true;
}

size_t Macro::removeHeldHotkeyKeys(const Hotkey& hk) {
  // Walk backwards. For each key: while we only see DOWN events (the held,
  // trailing presses incl. auto-repeat) mark them; stop at its first UP event.
  std::map<uint32_t, bool> finished;  // vk -> reached its UP event
  std::vector<bool> erase(events_.size(), false);
  size_t removed = 0;
  for (size_t i = events_.size(); i-- > 0;) {
    const InputEvent& e = events_[i];
    if (e.type != EventType::Key || !keyBelongsToHotkey(e.vk, hk)) continue;
    bool& done = finished[e.vk];
    if (done) continue;
    if (e.down) {
      erase[i] = true;
      ++removed;
    } else {
      done = true;
    }
  }
  if (removed == 0) return 0;
  std::vector<InputEvent> kept;
  kept.reserve(events_.size() - removed);
  for (size_t i = 0; i < events_.size(); ++i)
    if (!erase[i]) kept.push_back(events_[i]);
  events_.swap(kept);
  return removed;
}

void PressedTracker::apply(const InputEvent& ev) {
  if (ev.type == EventType::Key) {
    auto it = std::find_if(keys_.begin(), keys_.end(),
                           [&](const DownKey& k) { return k.vk == ev.vk; });
    if (ev.down) {
      if (it == keys_.end()) keys_.push_back({ev.vk, ev.scanCode, ev.extended});
    } else if (it != keys_.end()) {
      keys_.erase(it);
    }
  } else if (ev.type == EventType::MouseButton) {
    auto it = std::find(buttons_.begin(), buttons_.end(), ev.button);
    if (ev.down) {
      if (it == buttons_.end()) buttons_.push_back(ev.button);
    } else if (it != buttons_.end()) {
      buttons_.erase(it);
    }
  }
}

bool PressedTracker::isDown(const InputEvent& ev) const {
  if (ev.type == EventType::Key)
    return std::any_of(keys_.begin(), keys_.end(), [&](const DownKey& k) { return k.vk == ev.vk; });
  if (ev.type == EventType::MouseButton)
    return std::find(buttons_.begin(), buttons_.end(), ev.button) != buttons_.end();
  return false;
}

void PressedTracker::clear() {
  keys_.clear();
  buttons_.clear();
}

std::vector<InputEvent> PressedTracker::releaseEvents() const {
  std::vector<InputEvent> out;
  for (size_t i = keys_.size(); i-- > 0;) {
    InputEvent e;
    e.type = EventType::Key;
    e.down = false;
    e.vk = keys_[i].vk;
    e.scanCode = keys_[i].scanCode;
    e.extended = keys_[i].extended;
    out.push_back(e);
  }
  for (size_t i = buttons_.size(); i-- > 0;) {
    InputEvent e;
    e.type = EventType::MouseButton;
    e.down = false;
    e.button = buttons_[i];
    out.push_back(e);
  }
  return out;
}

}  // namespace macrolite
