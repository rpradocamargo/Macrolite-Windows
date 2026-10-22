// The Macrolite main window: owns the UI, the recorder, the player and the state.
#pragma once
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "core/events.h"
#include "core/hotkey.h"
#include "core/repeat.h"
#include "core/settings.h"
#include "core/state.h"
#include "win/input_player.h"
#include "win/input_recorder.h"

namespace macrolite {

class MainWindow {
 public:
  MainWindow();
  ~MainWindow();
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  bool create(HINSTANCE hInstance);
  int run();  // message loop; returns the exit code

 private:
  static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

  // window lifecycle
  bool onCreate();
  void onClose();
  void onCommand(int id, int code);

  // layout / drawing
  int S(int v) const;  // scale a 96-dpi value to the current DPI
  void createFonts();
  void layout();
  void resizeWindow(bool keepPosition, int x, int y);
  void paint();
  void drawButton(const DRAWITEMSTRUCT& dis);
  void drawCheckbox(const DRAWITEMSTRUCT& dis);
  LRESULT controlColor(HDC dc, HWND control);
  void refreshUi();
  void updateHotkeyDisplays();
  void setMessage(const std::wstring& text, bool isError);

  // actions
  void toggleRecording(bool viaHotkey);
  void startRecording();
  void stopRecording(bool viaHotkey);
  void startPlayback();
  void stopPlayback();
  void onPlaybackDone();
  void openDonatePage();
  bool commitRepeatText();  // validates the edit box; restores last good value if invalid
  void onUnlimitedToggled();
  void saveCurrentSettings();

  // hotkeys
  Hotkey& hotkeyRef(int index);
  const wchar_t* hotkeyLabel(int index) const;
  std::vector<int> registerAllHotkeys();  // returns indexes that could not be registered
  void unregisterAllHotkeys();
  void beginCapture(int index);
  void cancelCapture();
  bool consumeCaptureMessage(const MSG& msg);
  void handleCaptureKey(UINT vk);
  bool tryAssignHotkey(int index, const Hotkey& candidate);

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  UINT dpi_ = 96;

  HFONT fontNormal_ = nullptr;
  HFONT fontBold_ = nullptr;
  HFONT fontTitle_ = nullptr;
  HBRUSH brushBackground_ = nullptr;
  HBRUSH brushPanel_ = nullptr;
  HBRUSH brushDark_ = nullptr;

  RECT panels_[4] = {};
  RECT hotkeyFields_[3] = {};
  RECT editField_ = {};
  RECT statusBar_ = {};
  int playbackSpeedPercent_ = 100;

  StateMachine state_;
  InputRecorder recorder_;
  InputPlayer player_;
  std::shared_ptr<const Macro> macro_;  // last recording (shared read-only with the player)

  Settings settings_;
  RepeatSettings repeat_;
  bool updatingEdit_ = false;
  bool editFocused_ = false;

  int capturing_ = -1;  // index of the hotkey being captured, or -1
  bool registered_[3] = {false, false, false};

  std::wstring message_;
  bool messageIsError_ = false;
};

}  // namespace macrolite
