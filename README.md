# Macrolite

A small, dark-themed macro recorder for Windows 11. It records your mouse
(movement, buttons, scrolling) and keyboard (key presses and releases, including
Ctrl/Alt/Shift) with timing, and plays them back. Written in C++17 with the
plain Windows API. No third-party libraries.

> **Status honesty:** the logic in `src/core` was compiled and unit-tested
> (21 tests, 156 checks, all passing) on Linux with g++. The Windows code in
> `src/win` (window, hooks, playback) was syntax-checked but **has not yet been
> compiled or run on Windows**. The first `.\build.ps1` on your PC is the first
> real compile. If it fails, paste the full output and it will be fixed.

---

## 1. Prerequisites

You need two things: **CMake** and a **C++ compiler** (Visual Studio Build Tools
is recommended). Check what you have:

```powershell
Set-Location "D:\Macrolite"
Get-ChildItem -Recurse | Unblock-File
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\check-env.ps1
```

* `Unblock-File` removes the "downloaded from the internet" mark that stops
  PowerShell from running scripts.
* `Set-ExecutionPolicy -Scope Process` only affects **this PowerShell window**.
  You must repeat it each time you open a new window.

If `check-env.ps1` reports something missing, install it (about 3-6 GB, takes a
while):

```powershell
winget install --id Kitware.CMake -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Then **close PowerShell, open a new window**, and run `.\check-env.ps1` again.

MinGW (g++) also works, but it must be the *posix threads* variant
(`g++ -v` should show `Thread model: posix`).

## 2. Build, test, run

```powershell
Set-Location "D:\Macrolite"
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\build.ps1
```

On success you will see `BUILD SUCCEEDED`. Then:

```powershell
.\test.ps1     # builds and runs the automated tests
.\run.ps1      # launches Macrolite
```

Other options: `.\build.ps1 -Clean` (start from scratch), `.\build.ps1 -Config Debug`.

## 3. Project structure

```
D:\Macrolite
|-- CMakeLists.txt          build definition
|-- build.ps1 run.ps1 test.ps1 check-env.ps1
|-- src\core\               pure logic, no Windows code (unit tested)
|   |-- hotkey.*            hotkey model, names, validation, conflicts
|   |-- repeat.*            repeat-count validation, "Unlimited" model
|   |-- events.*            recorded events, Macro, pressed-key tracking
|   |-- state.h             Idle / Recording / Playing state machine
|   `-- settings.*          settings text format (load/save parsing)
|-- src\win\                Windows-specific code
|   |-- main.cpp            entry point, DPI setup, single instance
|   |-- main_window.*       window, drawing, buttons, hotkeys, wiring
|   |-- input_recorder.*    global mouse/keyboard hooks -> events
|   |-- input_player.*      background thread, SendInput playback
|   |-- settings_store.*    reads/writes %APPDATA%\Macrolite\settings.ini
|   `-- theme.h win_util.h  colours, string helper
`-- tests\test_main.cpp     automated tests for src\core
```

## 4. Using Macrolite

**Default hotkeys** (work while another program has focus):

| Action | Default |
|---|---|
| Record / Stop Recording | Ctrl + Shift + R |
| Playback | Ctrl + Shift + P |
| Stop Playback | Ctrl + Shift + S |

**Recording.** Click **Record** (or press the hotkey), switch to the program you
want to automate, do your actions, then click **Stop Recording** (or press the
hotkey). The button shows the status: *Idle*, *Recording*, *Playing Back*.
Clicks on Macrolite's own window and typing inside it are never recorded. If you
stop with the hotkey, the hotkey's own key presses are removed from the macro.
A new recording replaces the old one only if it actually captured something.

**Playback.** Click **Playback** (or the hotkey). The button becomes
**Stop Playback**. Macrolite waits until you release the hotkey's modifier keys
(otherwise your held Ctrl+Shift would change the replayed clicks). Stopping is
immediate, even in the middle of a long pause, and any keys or mouse buttons still
held by the macro are released.

**Repeat.** Type a number from 1 to 100000 in the box. Invalid values are rejected
and the last good value is restored. Tick **Unlimited** to repeat until you stop;
the box is grayed out, and unticking restores your number.

**Changing a hotkey.** Click **Change** next to it, then press the new combination
(for example Ctrl + Alt + F5). Press **Esc** (or click **Cancel**) to abort.
Rules: needs Ctrl/Alt/Shift/Win unless it is F1-F24; Shift + letter is refused
(it would break typing); Windows-reserved combos (Alt+F4, Alt+Tab, Ctrl+Alt+Del
...) are refused; two actions cannot share a hotkey; if another program already
owns the combination, Macrolite tells you and keeps your old one.

**Settings** (hotkeys, repeat count, unlimited) are saved automatically to
`%APPDATA%\Macrolite\settings.ini`. Delete that file to reset to defaults.
Macros themselves are **not** saved; they live in memory until you close the app.

**Donate** opens https://www.paypal.com/donate/?hosted_button_id=7V8NP5HAPYMB6
in your default browser.

## 5. Troubleshooting

| Problem | What to do |
|---|---|
| `running scripts is disabled on this system` | Run `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass` in that window. |
| `.ps1 is not digitally signed` / blocked | `Get-ChildItem -Recurse D:\Macrolite \| Unblock-File` |
| `CMake was not found` / `No C++ compiler found` | Install as in section 1, then open a **new** PowerShell. |
| Build fails | Run `.\build.ps1 -Clean`. If it still fails, copy the **entire** output. |
| "Could not register hotkey(s)" | Another program uses that combination. Click **Change** and pick another. |
| Playback does nothing in some program | See the limitations below (elevated windows, games). |
| Macrolite says it is already running | Look in the system tray / Task Manager for `macrolite.exe`. |
| Antivirus warns about the app | See limitations: keyboard hooks look like keyloggers to some scanners. |

## 6. Known limitations and Windows restrictions

* **Elevated (administrator) programs.** Windows blocks input from a normal
  program into a window running as administrator (UIPI), and `SendInput` does not
  report that failure. To record or control elevated programs, start Macrolite
  as administrator (right-click -> Run as administrator).
* **Secure desktop.** Nothing can be recorded or sent on the lock screen, UAC
  prompts, or Ctrl+Alt+Del screen.
* **Games and some apps** read input through raw input / DirectInput or use
  anti-cheat that ignores or blocks simulated input. Macrolite cannot work there,
  and using it in online games may violate their rules.
* **Coordinates are absolute screen pixels.** If windows move, or your
  resolution/DPI/monitor layout changes, replayed clicks land in the wrong place.
  Playback accuracy is roughly +/- 1 pixel on large screens.
* **Timing** is accurate to about a millisecond, but Windows may delay events
  when the PC is heavily loaded.
* **Low-level hooks** can be silently removed by Windows if the app stalls for
  too long. Stop and start recording again if capture ever seems dead.
* Keys are replayed by virtual-key code (plus scan code); IME/dead-key
  composition is not reproduced exactly.
* Win-key combinations as hotkeys may be intercepted by Windows before Macrolite
  sees them. Esc alone cancels hotkey capture, so it can't be a hotkey by itself.
* Recording is capped at 500,000 events; the app tells you if the cap is hit.
* Antivirus software may flag programs that install global keyboard hooks.
  Macrolite only records while you have pressed Record and keeps data in memory.
* Only one copy of Macrolite can run at a time.
