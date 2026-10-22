// Macrolite entry point.
#include <windows.h>

#include <commctrl.h>

#include "win/main_window.h"

namespace {

// Per-monitor DPI awareness (Windows 10 1703+), loaded dynamically so the
// program still starts on systems where the API is missing. Must run before
// any window is created. It also makes mouse coordinates physical pixels, which
// is what recording and playback need.
void enableDpiAwareness() {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (!user32) return;
  using Fn = BOOL(WINAPI*)(HANDLE);
  Fn setContext = reinterpret_cast<Fn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
  if (setContext) {
    setContext(reinterpret_cast<HANDLE>(-4));  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
  } else {
    SetProcessDPIAware();
  }
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
  enableDpiAwareness();

  // Only one copy at a time: two copies would fight over the global hotkeys.
  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\Macrolite.SingleInstance");
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(nullptr, L"Macrolite is already running.", L"Macrolite", MB_OK | MB_ICONINFORMATION);
    CloseHandle(mutex);
    return 0;
  }

  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  int exitCode = 1;
  {
    macrolite::MainWindow window;
    if (!window.create(instance)) {
      MessageBoxW(nullptr, L"Macrolite could not create its window.", L"Macrolite", MB_OK | MB_ICONERROR);
    } else {
      exitCode = window.run();
    }
  }
  if (mutex) CloseHandle(mutex);
  return exitCode;
}
