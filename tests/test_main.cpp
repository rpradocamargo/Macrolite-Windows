// Macrolite core tests. No external framework: build with CMake or any C++17 compiler.
#include <iostream>
#include <string>
#include <vector>

#include "core/events.h"
#include "core/hotkey.h"
#include "core/keys.h"
#include "core/repeat.h"
#include "core/settings.h"
#include "core/state.h"

using namespace macrolite;

namespace {

int g_failures = 0;
int g_checks = 0;

struct TestCase {
  const char* name;
  void (*fn)();
};
std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}
struct Registrar {
  Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); }
};

#define TEST(name)                       \
  static void name();                    \
  static Registrar registrar_##name(#name, name); \
  static void name()

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_failures;                                                              \
      std::cerr << "  FAILED " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
    }                                                                            \
  } while (0)

InputEvent key(int64_t t, uint32_t vk, bool down) {
  InputEvent e;
  e.type = EventType::Key;
  e.timeMs = t;
  e.vk = vk;
  e.down = down;
  return e;
}
InputEvent mouseMove(int64_t t, int x, int y) {
  InputEvent e;
  e.type = EventType::MouseMove;
  e.timeMs = t;
  e.x = x;
  e.y = y;
  return e;
}
InputEvent mouseBtn(int64_t t, MouseButton b, bool down) {
  InputEvent e;
  e.type = EventType::MouseButton;
  e.timeMs = t;
  e.button = b;
  e.down = down;
  return e;
}

// ---------------------------------------------------------------- repeat

TEST(repeat_accepts_valid_values) {
  CHECK(parseRepeatCount("1").ok && parseRepeatCount("1").value == 1);
  CHECK(parseRepeatCount("25").value == 25);
  CHECK(parseRepeatCount("  7  ").ok && parseRepeatCount("  7  ").value == 7);
  CHECK(parseRepeatCount("007").ok && parseRepeatCount("007").value == 7);
  CHECK(parseRepeatCount(std::to_string(kMaxRepeatCount)).ok);
}

TEST(repeat_rejects_invalid_values) {
  CHECK(!parseRepeatCount("").ok);
  CHECK(!parseRepeatCount("   ").ok);
  CHECK(!parseRepeatCount("0").ok);
  CHECK(!parseRepeatCount("000").ok);
  CHECK(!parseRepeatCount("-1").ok);
  CHECK(!parseRepeatCount("+5").ok);
  CHECK(!parseRepeatCount("abc").ok);
  CHECK(!parseRepeatCount("1.5").ok);
  CHECK(!parseRepeatCount("1 2").ok);
  CHECK(!parseRepeatCount("12a").ok);
  CHECK(!parseRepeatCount(std::to_string(kMaxRepeatCount + 1)).ok);
  CHECK(!parseRepeatCount("99999999999999999999999999").ok);  // no overflow
  CHECK(!parseRepeatCount("1").error.empty() == false);       // valid => no error text
  CHECK(!parseRepeatCount("x").error.empty());
}

TEST(repeat_settings_defaults_and_unlimited_restore) {
  RepeatSettings r;
  CHECK(r.count() == 1);
  CHECK(!r.unlimited());
  CHECK(r.playbackRepeats() == 1);

  CHECK(r.setCountText("5"));
  CHECK(r.playbackRepeats() == 5);

  r.setUnlimited(true);
  CHECK(r.playbackRepeats() == 0);  // 0 == until stopped
  CHECK(r.count() == 5);            // previous quantity preserved

  r.setUnlimited(false);
  CHECK(r.countText() == "5");      // restored
  CHECK(r.playbackRepeats() == 5);

  CHECK(!r.setCountText("junk"));   // invalid input does not overwrite
  CHECK(r.count() == 5);
  CHECK(!r.setCountText("0"));
  CHECK(r.count() == 5);
}

// ---------------------------------------------------------------- hotkeys

TEST(hotkey_default_names) {
  Settings s;
  CHECK(toString(s.record) == "Ctrl + Shift + R");
  CHECK(toString(s.playback) == "Ctrl + Shift + P");
  CHECK(toString(s.stopPlayback) == "Ctrl + Shift + S");
  CHECK(toString(Hotkey{mod::Ctrl | mod::Alt | mod::Shift, vkc::F1}) == "Ctrl + Alt + Shift + F1");
  CHECK(toString(Hotkey{mod::Win, 'Q'}) == "Win + Q");
}

TEST(hotkey_defaults_are_valid_and_distinct) {
  Settings s;
  CHECK(validateHotkey(s.record).ok);
  CHECK(validateHotkey(s.playback).ok);
  CHECK(validateHotkey(s.stopPlayback).ok);
  CHECK(s.record != s.playback && s.record != s.stopPlayback && s.playback != s.stopPlayback);
}

TEST(hotkey_validation_rules) {
  CHECK(!validateHotkey(Hotkey{mod::Ctrl, 0}).ok);                       // no key
  CHECK(!validateHotkey(Hotkey{mod::Ctrl, vkc::Shift}).ok);              // modifier as key
  CHECK(!validateHotkey(Hotkey{mod::Ctrl, vkc::LControl}).ok);
  CHECK(!validateHotkey(Hotkey{0, 'A'}).ok);                             // bare letter
  CHECK(!validateHotkey(Hotkey{0, vkc::Escape}).ok);                     // bare Esc
  CHECK(validateHotkey(Hotkey{0, vkc::F1}).ok);                          // bare F-key ok
  CHECK(validateHotkey(Hotkey{0, vkc::F24}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Shift, 'A'}).ok);                    // blocks typing
  CHECK(!validateHotkey(Hotkey{mod::Shift, vkc::Space}).ok);
  CHECK(validateHotkey(Hotkey{mod::Shift, vkc::F1 + 4}).ok);
  CHECK(validateHotkey(Hotkey{mod::Ctrl, 'A'}).ok);
  CHECK(validateHotkey(Hotkey{mod::Alt, 'A'}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Alt, vkc::F4}).ok);                  // reserved
  CHECK(!validateHotkey(Hotkey{mod::Alt, vkc::Tab}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Ctrl | mod::Alt, vkc::Delete}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Ctrl | mod::Shift, vkc::Escape}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Win, 'L'}).ok);
  CHECK(!validateHotkey(Hotkey{0x10, 'A'}).ok);                          // unknown modifier bit
  CHECK(!validateHotkey(Hotkey{mod::Ctrl, 0xFF}).ok);
  CHECK(!validateHotkey(Hotkey{mod::Ctrl, 'A'}).message.empty() == false);
  CHECK(!validateHotkey(Hotkey{0, 'A'}).message.empty());
}

TEST(hotkey_conflict_detection) {
  Settings s;
  std::vector<NamedHotkey> others = {{"Playback", s.playback}, {"Stop Playback", s.stopPlayback}};
  CHECK(checkConflict(s.record, others).ok);
  ValidationResult r = checkConflict(s.playback, others);
  CHECK(!r.ok);
  CHECK(r.message.find("Playback") != std::string::npos);
  CHECK(checkConflict(Hotkey{mod::Ctrl, 'P'}, others).ok);  // different modifiers => no conflict
}

TEST(hotkey_key_names) {
  CHECK(keyName('A') == "A");
  CHECK(keyName('7') == "7");
  CHECK(keyName(0x75) == "F6");
  CHECK(keyName(0x21) == "Page Up");
  CHECK(keyName(0x60) == "Num 0");
  CHECK(keyName(0xFE) == "Key 0xFE");
}

// ---------------------------------------------------------------- macro / events

TEST(macro_preserves_order_and_timing) {
  Macro m;
  CHECK(m.empty());
  CHECK(m.append(mouseMove(0, 10, 10)));
  CHECK(m.append(mouseBtn(15, MouseButton::Left, true)));
  CHECK(m.append(mouseBtn(15, MouseButton::Left, false)));  // equal timestamps keep order
  CHECK(m.append(key(120, 'A', true)));
  CHECK(m.append(key(180, 'A', false)));
  CHECK(m.size() == 5);
  CHECK(m.durationMs() == 180);
  const auto& ev = m.events();
  CHECK(ev[1].down && !ev[2].down);
  CHECK(ev[3].vk == 'A' && ev[3].down && ev[4].vk == 'A' && !ev[4].down);
  CHECK(ev[3].timeMs - ev[2].timeMs == 105);  // relative timing preserved
  for (size_t i = 1; i < ev.size(); ++i) CHECK(ev[i].timeMs >= ev[i - 1].timeMs);
}

TEST(macro_rejects_out_of_order_and_negative_times) {
  Macro m;
  CHECK(m.append(key(100, 'A', true)));
  CHECK(!m.append(key(99, 'A', false)));  // earlier than previous
  CHECK(!m.append(key(-1, 'B', true)));   // negative
  CHECK(m.size() == 1);
  CHECK(m.append(key(100, 'A', false)));  // equal is fine
  CHECK(m.size() == 2);
}

TEST(pressed_tracker_tracks_and_releases) {
  PressedTracker t;
  CHECK(!t.anyDown());
  t.apply(key(0, vkc::LControl, true));
  t.apply(key(1, 'C', true));
  t.apply(mouseBtn(2, MouseButton::Right, true));
  CHECK(t.anyDown());
  CHECK(t.isDown(key(0, 'C', false)));
  CHECK(t.isDown(mouseBtn(0, MouseButton::Right, false)));
  CHECK(!t.isDown(mouseBtn(0, MouseButton::Left, false)));

  t.apply(key(3, 'C', false));
  CHECK(!t.isDown(key(0, 'C', false)));

  std::vector<InputEvent> rel = t.releaseEvents();
  CHECK(rel.size() == 2);
  bool sawCtrl = false, sawRight = false;
  for (const auto& e : rel) {
    CHECK(!e.down);
    if (e.type == EventType::Key && e.vk == vkc::LControl) sawCtrl = true;
    if (e.type == EventType::MouseButton && e.button == MouseButton::Right) sawRight = true;
  }
  CHECK(sawCtrl && sawRight);

  t.clear();
  CHECK(!t.anyDown());
  CHECK(t.releaseEvents().empty());
}

TEST(pressed_tracker_ignores_duplicate_downs_and_stray_ups) {
  PressedTracker t;
  t.apply(key(0, 'A', true));
  t.apply(key(1, 'A', true));  // auto-repeat
  CHECK(t.releaseEvents().size() == 1);
  t.apply(key(2, 'B', false));  // stray up
  CHECK(t.releaseEvents().size() == 1);
  t.apply(key(3, 'A', false));
  CHECK(!t.anyDown());
}

TEST(pressed_tracker_filters_orphan_ups_like_the_recorder_does) {
  // The recorder drops key-UP events whose DOWN it never saw (e.g. the release
  // of the hotkey that started recording).
  PressedTracker t;
  Macro m;
  std::vector<InputEvent> raw = {key(0, vkc::LShift, false), key(1, 'R', false), key(5, 'A', true),
                                 key(9, 'A', false)};
  for (const auto& e : raw) {
    if (!e.down && !t.isDown(e)) continue;
    t.apply(e);
    m.append(e);
  }
  CHECK(m.size() == 2);
  CHECK(m.events()[0].vk == 'A');
}

TEST(macro_removes_held_hotkey_keys_at_stop) {
  Hotkey hk{mod::Ctrl | mod::Shift, 'R'};
  Macro m;
  m.append(key(0, 'A', true));
  m.append(key(10, 'A', false));
  m.append(key(500, vkc::LControl, true));
  m.append(key(510, vkc::LShift, true));
  m.append(key(520, 'R', true));
  m.append(key(560, 'R', true));  // auto-repeat
  CHECK(m.removeHeldHotkeyKeys(hk) == 4);
  CHECK(m.size() == 2);
  CHECK(m.events()[0].vk == 'A' && m.events()[1].vk == 'A');
}

TEST(macro_keeps_released_keys_when_trimming_hotkey) {
  // User legitimately pressed and released Ctrl earlier: must stay intact.
  Hotkey hk{mod::Ctrl | mod::Shift, 'R'};
  Macro m;
  m.append(key(0, vkc::LControl, true));
  m.append(key(5, 'C', true));
  m.append(key(9, 'C', false));
  m.append(key(12, vkc::LControl, false));
  m.append(key(300, vkc::LControl, true));
  m.append(key(310, vkc::LShift, true));
  m.append(key(320, 'R', true));
  CHECK(m.removeHeldHotkeyKeys(hk) == 3);
  CHECK(m.size() == 4);
  CHECK(m.events()[0].vk == vkc::LControl && m.events()[0].down);
  CHECK(m.events()[3].vk == vkc::LControl && !m.events()[3].down);
}

TEST(macro_trim_with_nothing_to_remove) {
  Macro m;
  m.append(key(0, 'A', true));
  m.append(key(1, 'A', false));
  CHECK(m.removeHeldHotkeyKeys(Hotkey{mod::Ctrl, 'R'}) == 0);
  CHECK(m.size() == 2);
}

// ---------------------------------------------------------------- state machine

TEST(state_machine_valid_transitions) {
  StateMachine s;
  CHECK(s.state() == AppState::Idle);
  CHECK(s.beginRecording());
  CHECK(s.state() == AppState::Recording);
  CHECK(s.endRecording());
  CHECK(s.state() == AppState::Idle);
  CHECK(s.beginPlayback());
  CHECK(s.state() == AppState::Playing);
  CHECK(s.endPlayback());
  CHECK(s.state() == AppState::Idle);
}

TEST(state_machine_rejects_invalid_transitions) {
  StateMachine s;
  CHECK(!s.endRecording());   // not recording
  CHECK(!s.endPlayback());    // not playing
  CHECK(s.beginRecording());
  CHECK(!s.beginRecording()); // already recording
  CHECK(!s.beginPlayback());  // cannot play while recording
  CHECK(!s.endPlayback());
  CHECK(s.state() == AppState::Recording);
  CHECK(s.endRecording());
  CHECK(s.beginPlayback());
  CHECK(!s.beginPlayback());  // only one playback at a time
  CHECK(!s.beginRecording()); // cannot record while playing
  CHECK(!s.endRecording());
  CHECK(s.state() == AppState::Playing);
  CHECK(std::string(toString(AppState::Idle)) == "Idle");
  CHECK(std::string(toString(AppState::Recording)) == "Recording");
  CHECK(std::string(toString(AppState::Playing)) == "Playing Back");
}

// ---------------------------------------------------------------- settings

TEST(settings_roundtrip) {
  Settings s;
  s.record = Hotkey{mod::Ctrl | mod::Alt, vkc::F1 + 4};  // Ctrl+Alt+F5
  s.repeatCount = 42;
  s.unlimited = true;
  Settings p = parseSettings(serializeSettings(s));
  CHECK(p.record == s.record);
  CHECK(p.playback == s.playback);
  CHECK(p.stopPlayback == s.stopPlayback);
  CHECK(p.repeatCount == 42);
  CHECK(p.unlimited);
}

TEST(settings_fall_back_to_defaults_on_garbage) {
  Settings d;
  Settings p = parseSettings("this is not\nrecord_hotkey=abc\nrepeat_count=-9\nunlimited=maybe\n\n=\n");
  CHECK(p.record == d.record);
  CHECK(p.repeatCount == 1);
  CHECK(!p.unlimited);
  Settings e = parseSettings("");
  CHECK(e.record == d.record && e.playback == d.playback && e.stopPlayback == d.stopPlayback);
}

TEST(settings_reject_invalid_hotkeys_and_conflicts) {
  Settings d;
  // bare 'A' is invalid -> default kept
  Settings p = parseSettings("record_hotkey=0,65\n");
  CHECK(p.record == d.record);
  // all three identical -> conflict -> defaults for all
  Settings c = parseSettings("record_hotkey=2,65\nplayback_hotkey=2,65\nstop_hotkey=2,65\n");
  CHECK(c.record == d.record && c.playback == d.playback && c.stopPlayback == d.stopPlayback);
  // overflow-length number rejected
  Settings o = parseSettings("record_hotkey=99999999999999,65\n");
  CHECK(o.record == d.record);
}

}  // namespace

int main() {
  for (const TestCase& t : registry()) {
    int before = g_failures;
    t.fn();
    std::cout << (g_failures == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
  }
  std::cout << "\n" << registry().size() << " tests, " << g_checks << " checks, " << g_failures
            << " failures\n";
  return g_failures == 0 ? 0 : 1;
}
