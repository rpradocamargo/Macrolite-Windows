#include "win/main_window.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>

#include "win/settings_store.h"
#include "win/theme.h"
#include "win/win_util.h"

namespace macrolite {

using namespace theme;

namespace {

const char* const kHotkeyNames[3] = {"Record / Stop Recording", "Playback", "Stop Playback"};

const wchar_t kClassName[] = L"MacroliteMainWindow";
const wchar_t kHoverProp[] = L"MacroliteHover";
const wchar_t kDonateUrl[] = L"https://www.paypal.com/donate/?hosted_button_id=7V8NP5HAPYMB6";

constexpr UINT kMsgPlaybackDone = WM_APP + 1;

// Hotkey ids passed to RegisterHotKey: index + 1.
enum : int {
  IDC_TITLE = 100,
  IDC_RECORD = 101,
  IDC_PLAYBACK = 102,
  IDC_SEC_MOUSE = 103,
  IDC_SEC_HOTKEYS = 104,
  IDC_SEC_REPEAT = 105,
  IDC_REPEAT_LABEL = 106,
  IDC_REPEAT_EDIT = 107,
  IDC_UNLIMITED = 108,
  IDC_DONATE = 109,
  IDC_STATUS = 110,
  IDC_MESSAGE = 111,
  IDC_HK_LABEL0 = 120,  // 120..122
  IDC_HK_FIELD0 = 130,  // 130..132
  IDC_HK_BTN0 = 140,    // 140..142
  IDC_SEC_SPEED = 150,
  IDC_SPEED_SLIDER = 151,
  IDC_SPEED_VALUE = 152
};

// Layout, in 96-dpi units.
constexpr int kClientW = 420;
constexpr int kClientH = 572;

static_assert(mod::Alt == MOD_ALT && mod::Ctrl == MOD_CONTROL && mod::Shift == MOD_SHIFT &&
                  mod::Win == MOD_WIN,
              "modifier constants must match Windows");

// Optional newer user32 APIs (Windows 10 1607+), loaded dynamically.
struct DpiApi {
  using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
  using AdjustFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
  GetDpiForWindowFn getDpiForWindow = nullptr;
  AdjustFn adjustWindowRectExForDpi = nullptr;
  DpiApi() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
    adjustWindowRectExForDpi =
        reinterpret_cast<AdjustFn>(GetProcAddress(user32, "AdjustWindowRectExForDpi"));
  }
};
const DpiApi& dpiApi() {
  static DpiApi api;
  return api;
}

COLORREF blend(COLORREF a, COLORREF b, int percentOfB) {
  auto mix = [&](int x, int y) { return (x * (100 - percentOfB) + y * percentOfB) / 100; };
  return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)),
             mix(GetBValue(a), GetBValue(b)));
}

void fillRound(HDC dc, const RECT& rc, int radius, COLORREF fill, COLORREF border) {
  HBRUSH brush = CreateSolidBrush(fill);
  HPEN pen = CreatePen(PS_SOLID, 1, border);
  HGDIOBJ oldBrush = SelectObject(dc, brush);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
  SelectObject(dc, oldPen);
  SelectObject(dc, oldBrush);
  DeleteObject(pen);
  DeleteObject(brush);
}

void fillSolid(HDC dc, const RECT& rc, COLORREF color) {
  HBRUSH b = CreateSolidBrush(color);
  FillRect(dc, &rc, b);
  DeleteObject(b);
}

std::wstring formatSeconds(int64_t ms) {
  int64_t hundredths = (ms % 1000) / 10;
  std::wstring frac = std::to_wstring(hundredths);
  if (frac.size() < 2) frac = L"0" + frac;
  return std::to_wstring(ms / 1000) + L"." + frac + L" s";
}

// Subclass for owner-drawn buttons: tracks mouse hover so we can highlight.
LRESULT CALLBACK buttonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                    UINT_PTR id, DWORD_PTR /*refData*/) {
  switch (msg) {
    case WM_MOUSEMOVE:
      if (!GetPropW(hwnd, kHoverProp)) {
        SetPropW(hwnd, kHoverProp, reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&t);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      break;
    case WM_MOUSELEAVE:
      RemovePropW(hwnd, kHoverProp);
      InvalidateRect(hwnd, nullptr, FALSE);
      break;
    case WM_NCDESTROY:
      RemovePropW(hwnd, kHoverProp);
      RemoveWindowSubclass(hwnd, buttonSubclassProc, id);
      break;
    default:
      break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

}  // namespace

// ------------------------------------------------------------------ lifecycle

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
  player_.requestStop();
  player_.join();
  if (fontNormal_) DeleteObject(fontNormal_);
  if (fontBold_) DeleteObject(fontBold_);
  if (fontTitle_) DeleteObject(fontTitle_);
  if (brushBackground_) DeleteObject(brushBackground_);
  if (brushPanel_) DeleteObject(brushPanel_);
  if (brushDark_) DeleteObject(brushDark_);
}

bool MainWindow::create(HINSTANCE hInstance) {
  instance_ = hInstance;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &MainWindow::wndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
  wc.lpszClassName = kClassName;
  if (!RegisterClassExW(&wc)) return false;

  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
  HWND hwnd = CreateWindowExW(0, kClassName, L"Macrolite", style, CW_USEDEFAULT, CW_USEDEFAULT, 480,
                              600, nullptr, nullptr, hInstance, this);
  if (!hwnd) return false;
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  return true;
}

int MainWindow::run() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (capturing_ >= 0 && consumeCaptureMessage(msg)) continue;
    if (!IsDialogMessageW(hwnd_, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  MainWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = static_cast<MainWindow*>(cs->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
  return self->handleMessage(msg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return onCreate() ? 0 : -1;
    case WM_ERASEBKGND:
      return 1;  // everything is painted (double-buffered) in WM_PAINT
    case WM_PAINT:
      paint();
      return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
      return controlColor(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
    case WM_DRAWITEM: {
      const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
      if (dis && dis->CtlType == ODT_BUTTON) {
        if (dis->CtlID == IDC_UNLIMITED) drawCheckbox(*dis);
        else drawButton(*dis);
        return TRUE;
      }
      break;
    }
    case WM_COMMAND:
      onCommand(LOWORD(wParam), HIWORD(wParam));
      return 0;
    case WM_HSCROLL:
      if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd_, IDC_SPEED_SLIDER)) {
        playbackSpeedPercent_ = static_cast<int>(SendMessageW(GetDlgItem(hwnd_, IDC_SPEED_SLIDER), TBM_GETPOS, 0, 0));
        wchar_t value[16];
        swprintf_s(value, L"%d%%", playbackSpeedPercent_);
        SetWindowTextW(GetDlgItem(hwnd_, IDC_SPEED_VALUE), value);
      }
      return 0;
    case WM_HOTKEY:
      switch (static_cast<int>(wParam)) {
        case 1: toggleRecording(true); break;
        case 2: startPlayback(); break;
        case 3: stopPlayback(); break;
        default: break;
      }
      return 0;
    case kMsgPlaybackDone:
      onPlaybackDone();
      return 0;
    case WM_DPICHANGED: {
      dpi_ = HIWORD(wParam);
      const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
      createFonts();
      layout();
      resizeWindow(true, suggested->left, suggested->top);
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    }
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE && capturing_ >= 0) cancelCapture();
      break;
    case WM_CLOSE:
      onClose();
      return 0;
    case WM_DESTROY:
      unregisterAllHotkeys();
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

bool MainWindow::onCreate() {
  brushBackground_ = CreateSolidBrush(kBackground);
  brushPanel_ = CreateSolidBrush(kSurface);
  brushDark_ = CreateSolidBrush(kDark);

  dpi_ = dpiApi().getDpiForWindow ? dpiApi().getDpiForWindow(hwnd_) : 96;
  if (dpi_ == 0) dpi_ = 96;

  auto makeStatic = [&](int id, const wchar_t* text, DWORD extraStyle) {
    CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | extraStyle, 0, 0, 10,
                    10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
  };
  auto makeButton = [&](int id, const wchar_t* text) {
    HWND b = CreateWindowExW(0, L"BUTTON", text,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10,
                             hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_,
                             nullptr);
    if (b) SetWindowSubclass(b, buttonSubclassProc, 1, 0);
  };

  makeStatic(IDC_TITLE, L"Macrolite", SS_LEFT | SS_CENTERIMAGE);
  makeStatic(IDC_SEC_MOUSE, L"MOUSE CONTROLS", SS_LEFT);
  makeButton(IDC_RECORD, L"Record");
  makeButton(IDC_PLAYBACK, L"Playback");

  makeStatic(IDC_SEC_HOTKEYS, L"KEYBOARD HOTKEYS", SS_LEFT);
  for (int i = 0; i < 3; ++i) {
    makeStatic(IDC_HK_LABEL0 + i, hotkeyLabel(i), SS_LEFT | SS_CENTERIMAGE);
    makeStatic(IDC_HK_FIELD0 + i, L"", SS_CENTER | SS_CENTERIMAGE);
    makeButton(IDC_HK_BTN0 + i, L"Change");
  }

  makeStatic(IDC_SEC_REPEAT, L"REPEAT", SS_LEFT);
  makeStatic(IDC_REPEAT_LABEL, L"Repeat", SS_LEFT | SS_CENTERIMAGE);
  HWND edit = CreateWindowExW(0, L"EDIT", L"1",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 0, 0,
                              10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_REPEAT_EDIT)),
                              instance_, nullptr);
  if (edit) SendMessageW(edit, EM_LIMITTEXT, 6, 0);
  makeButton(IDC_UNLIMITED, L"Unlimited");

  makeStatic(IDC_SEC_SPEED, L"PLAYBACK SPEED", SS_LEFT);
  HWND speed = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS | TBS_NOTICKS, 0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SPEED_SLIDER)), instance_, nullptr);
  if (speed) {
    SendMessageW(speed, TBM_SETRANGE, TRUE, MAKELONG(25, 200));
    SendMessageW(speed, TBM_SETPOS, TRUE, playbackSpeedPercent_);
    SendMessageW(speed, TBM_SETTICFREQ, 25, 0);
  }
  makeStatic(IDC_SPEED_VALUE, L"100%", SS_RIGHT | SS_CENTERIMAGE);

  makeButton(IDC_DONATE, L"Donate");
  makeStatic(IDC_STATUS, L"Idle", SS_LEFT | SS_CENTERIMAGE);
  makeStatic(IDC_MESSAGE, L"", SS_LEFT);

  // Dark title bar (Windows 10 2004+ / Windows 11). Errors are ignored on older systems.
  BOOL useDark = TRUE;
  DwmSetWindowAttribute(hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDark, sizeof(useDark));
  COLORREF caption = kDark;
  DwmSetWindowAttribute(hwnd_, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
  COLORREF captionText = kText;
  DwmSetWindowAttribute(hwnd_, 36 /*DWMWA_TEXT_COLOR*/, &captionText, sizeof(captionText));

  // Settings
  settings_ = loadSettings();
  repeat_.setCountText(std::to_string(settings_.repeatCount));
  repeat_.setUnlimited(settings_.unlimited);
  updatingEdit_ = true;
  SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_EDIT), widen(repeat_.countText()).c_str());
  updatingEdit_ = false;

  createFonts();
  layout();
  resizeWindow(false, 0, 0);
  updateHotkeyDisplays();

  std::vector<int> failed = registerAllHotkeys();
  if (failed.empty()) {
    setMessage(L"Ready. Press " + widen(toString(settings_.record)) + L" to start recording.", false);
  } else {
    std::wstring names;
    for (int i : failed) {
      if (!names.empty()) names += L", ";
      names += hotkeyLabel(i);
    }
    setMessage(L"Could not register hotkey(s): " + names +
                   L". Another program may be using them. Use Change to pick other keys.",
               true);
  }
  refreshUi();
  return true;
}

void MainWindow::onClose() {
  capturing_ = -1;
  if (state_.state() == AppState::Recording) {
    recorder_.stop();
    state_.endRecording();
  }
  if (state_.state() == AppState::Playing) {
    player_.requestStop();
    player_.join();
    state_.endPlayback();
  }
  saveCurrentSettings();
  DestroyWindow(hwnd_);
}

void MainWindow::saveCurrentSettings() {
  settings_.repeatCount = repeat_.count();
  settings_.unlimited = repeat_.unlimited();
  saveSettings(settings_);  // best effort
}

// ------------------------------------------------------------------ layout & drawing

int MainWindow::S(int v) const { return MulDiv(v, static_cast<int>(dpi_), 96); }

void MainWindow::createFonts() {
  if (fontNormal_) DeleteObject(fontNormal_);
  if (fontBold_) DeleteObject(fontBold_);
  if (fontTitle_) DeleteObject(fontTitle_);
  auto make = [&](int pt, int weight) {
    return CreateFontW(-MulDiv(pt, static_cast<int>(dpi_), 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  };
  fontNormal_ = make(9, FW_NORMAL);
  fontBold_ = make(9, FW_SEMIBOLD);
  fontTitle_ = make(17, FW_SEMIBOLD);
}

void MainWindow::layout() {
  auto place = [&](int id, int x, int y, int w, int h, HFONT font) {
    HWND c = GetDlgItem(hwnd_, id);
    if (!c) return;
    SetWindowPos(c, nullptr, S(x), S(y), S(w), S(h), SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  };
  auto rect = [&](int l, int t, int r, int b) { return RECT{S(l), S(t), S(r), S(b)}; };

  place(IDC_TITLE, 16, 12, 300, 34, fontTitle_);

  // Mouse controls
  panels_[0] = rect(16, 56, 404, 136);
  place(IDC_SEC_MOUSE, 28, 64, 250, 16, fontBold_);
  place(IDC_RECORD, 28, 88, 176, 36, fontBold_);
  place(IDC_PLAYBACK, 216, 88, 176, 36, fontBold_);

  // Hotkeys
  panels_[1] = rect(16, 148, 404, 292);
  place(IDC_SEC_HOTKEYS, 28, 156, 250, 16, fontBold_);
  for (int i = 0; i < 3; ++i) {
    int y = 180 + 36 * i;
    place(IDC_HK_LABEL0 + i, 28, y, 154, 28, fontNormal_);
    hotkeyFields_[i] = rect(186, y, 314, y + 28);
    place(IDC_HK_FIELD0 + i, 186, y, 128, 28, fontNormal_);
    place(IDC_HK_BTN0 + i, 322, y, 70, 28, fontNormal_);
  }

  // Repeat
  panels_[2] = rect(16, 304, 404, 376);
  place(IDC_SEC_REPEAT, 28, 312, 250, 16, fontBold_);
  place(IDC_REPEAT_LABEL, 28, 336, 56, 28, fontNormal_);
  editField_ = rect(90, 336, 170, 364);
  place(IDC_REPEAT_EDIT, 98, 341, 64, 18, fontNormal_);
  place(IDC_UNLIMITED, 186, 336, 150, 28, fontNormal_);

  // Playback speed
  panels_[3] = rect(16, 388, 404, 448);
  place(IDC_SEC_SPEED, 28, 396, 250, 16, fontBold_);
  place(IDC_SPEED_SLIDER, 28, 416, 326, 24, fontNormal_);
  place(IDC_SPEED_VALUE, 354, 414, 38, 26, fontBold_);

  // Donate
  place(IDC_DONATE, 16, 460, 388, 40, fontBold_);

  // Status bar
  statusBar_ = rect(0, 512, kClientW, kClientH);
  place(IDC_STATUS, 40, 520, 358, 20, fontBold_);
  place(IDC_MESSAGE, 22, 544, 376, 24, fontNormal_);
}

void MainWindow::resizeWindow(bool keepPosition, int x, int y) {
  RECT r = {0, 0, S(kClientW), S(kClientH)};
  DWORD style = static_cast<DWORD>(GetWindowLongW(hwnd_, GWL_STYLE));
  DWORD exStyle = static_cast<DWORD>(GetWindowLongW(hwnd_, GWL_EXSTYLE));
  if (dpiApi().adjustWindowRectExForDpi)
    dpiApi().adjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi_);
  else
    AdjustWindowRectEx(&r, style, FALSE, exStyle);
  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (keepPosition ? 0 : SWP_NOMOVE);
  SetWindowPos(hwnd_, nullptr, x, y, r.right - r.left, r.bottom - r.top, flags);
}

void MainWindow::paint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  RECT client;
  GetClientRect(hwnd_, &client);
  int w = client.right - client.left;
  int h = client.bottom - client.top;

  HDC dc = CreateCompatibleDC(hdc);
  HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
  HGDIOBJ oldBmp = SelectObject(dc, bmp);

  FillRect(dc, &client, brushBackground_);
  for (const RECT& p : panels_) fillRound(dc, p, S(10), kSurface, kSurface);

  for (int i = 0; i < 3; ++i) {
    RECT f = hotkeyFields_[i];
    if (capturing_ == i) {
      RECT outer = f;
      InflateRect(&outer, S(1), S(1));
      fillSolid(dc, outer, kLightAccent);
    }
    fillSolid(dc, f, kDark);
  }
  {
    RECT f = editField_;
    if (editFocused_) {
      RECT outer = f;
      InflateRect(&outer, S(1), S(1));
      fillSolid(dc, outer, kLightAccent);
    }
    fillSolid(dc, f, kDark);
  }

  fillSolid(dc, statusBar_, kDark);

  // Status dot
  AppState s = state_.state();
  COLORREF dot = (s == AppState::Recording) ? kRecording : (s == AppState::Playing) ? kPlaying : kAccent;
  HBRUSH db = CreateSolidBrush(dot);
  HPEN dp = CreatePen(PS_SOLID, 1, dot);
  HGDIOBJ ob = SelectObject(dc, db);
  HGDIOBJ op = SelectObject(dc, dp);
  int cx = S(28), cy = S(458), r = S(5);
  Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
  SelectObject(dc, op);
  SelectObject(dc, ob);
  DeleteObject(dp);
  DeleteObject(db);

  BitBlt(hdc, 0, 0, w, h, dc, 0, 0, SRCCOPY);
  SelectObject(dc, oldBmp);
  DeleteObject(bmp);
  DeleteDC(dc);
  EndPaint(hwnd_, &ps);
}

LRESULT MainWindow::controlColor(HDC dc, HWND control) {
  int id = GetDlgCtrlID(control);
  COLORREF fg = kText;
  COLORREF bg = kSurface;
  HBRUSH brush = brushPanel_;

  if (id == IDC_TITLE) {
    bg = kBackground;
    brush = brushBackground_;
  } else if (id == IDC_SEC_MOUSE || id == IDC_SEC_HOTKEYS || id == IDC_SEC_REPEAT || id == IDC_SEC_SPEED) {
    fg = kLightAccent;
  } else if (id >= IDC_HK_FIELD0 && id < IDC_HK_FIELD0 + 3) {
    bg = kDark;
    brush = brushDark_;
    if (capturing_ == id - IDC_HK_FIELD0) fg = kLightAccent;
  } else if (id == IDC_REPEAT_EDIT) {
    bg = kDark;
    brush = brushDark_;
    if (!IsWindowEnabled(control)) fg = kDisabledText;
  } else if (id == IDC_STATUS) {
    bg = kDark;
    brush = brushDark_;
  } else if (id == IDC_MESSAGE) {
    bg = kDark;
    brush = brushDark_;
    fg = messageIsError_ ? kRecording : kMuted;
  }
  SetTextColor(dc, fg);
  SetBkColor(dc, bg);
  SetBkMode(dc, OPAQUE);
  return reinterpret_cast<LRESULT>(brush);
}

void MainWindow::drawButton(const DRAWITEMSTRUCT& dis) {
  HDC dc = dis.hDC;
  RECT rc = dis.rcItem;
  const int id = static_cast<int>(dis.CtlID);
  const bool disabled = (dis.itemState & ODS_DISABLED) != 0;
  const bool pressed = (dis.itemState & ODS_SELECTED) != 0;
  const bool focused = (dis.itemState & ODS_FOCUS) != 0;
  const bool hover = GetPropW(dis.hwndItem, kHoverProp) != nullptr;
  const AppState s = state_.state();

  wchar_t text[64] = L"";
  GetWindowTextW(dis.hwndItem, text, 64);

  COLORREF parentBg = (id == IDC_DONATE) ? kBackground : kSurface;
  COLORREF fill = kDark;
  COLORREF fg = kText;
  bool primary = false;

  if (id == IDC_RECORD) {
    fill = (s == AppState::Recording) ? kRecording : kAccent;
    fg = kDark;
    primary = true;
  } else if (id == IDC_PLAYBACK) {
    fill = (s == AppState::Playing) ? kPlaying : kAccent;
    fg = kDark;
    primary = true;
  } else if (id == IDC_DONATE) {
    fill = kLightAccent;
    fg = kBlack;
    primary = true;
  } else if (id >= IDC_HK_BTN0 && id < IDC_HK_BTN0 + 3 && capturing_ == id - IDC_HK_BTN0) {
    fill = kAccent;
    fg = kDark;
  }

  if (disabled) {
    fill = blend(kSurface, kDark, 45);
    fg = kDisabledText;
  } else if (pressed) {
    fill = blend(fill, RGB(0, 0, 0), 22);
  } else if (hover) {
    fill = primary ? blend(fill, RGB(255, 255, 255), 18) : blend(fill, kAccent, 25);
  }

  fillSolid(dc, rc, parentBg);
  COLORREF border = (focused && !disabled) ? kLightAccent : fill;
  fillRound(dc, rc, S(6), fill, border);

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, fg);
  HGDIOBJ oldFont = SelectObject(dc, primary ? fontBold_ : fontNormal_);
  DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(dc, oldFont);
}

void MainWindow::drawCheckbox(const DRAWITEMSTRUCT& dis) {
  HDC dc = dis.hDC;
  RECT rc = dis.rcItem;
  const bool disabled = (dis.itemState & ODS_DISABLED) != 0;
  const bool focused = (dis.itemState & ODS_FOCUS) != 0;
  const bool hover = GetPropW(dis.hwndItem, kHoverProp) != nullptr;
  const bool checked = repeat_.unlimited();

  fillSolid(dc, rc, kSurface);

  int box = S(18);
  int top = rc.top + ((rc.bottom - rc.top) - box) / 2;
  RECT b = {rc.left + S(2), top, rc.left + S(2) + box, top + box};

  COLORREF border = disabled ? kDisabledText : (focused || hover) ? kLightAccent : kAccent;
  COLORREF fill = checked ? (disabled ? kDisabledText : kAccent) : kDark;
  fillRound(dc, b, S(4), fill, border);

  if (checked) {
    HPEN pen = CreatePen(PS_SOLID, std::max(2, S(2)), kDark);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, b.left + S(4), b.top + S(9), nullptr);
    LineTo(dc, b.left + S(8), b.top + S(13));
    LineTo(dc, b.left + S(14), b.top + S(5));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  wchar_t text[64] = L"";
  GetWindowTextW(dis.hwndItem, text, 64);
  RECT t = rc;
  t.left = b.right + S(10);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, disabled ? kDisabledText : kText);
  HGDIOBJ oldFont = SelectObject(dc, fontNormal_);
  DrawTextW(dc, text, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(dc, oldFont);
}

void MainWindow::setMessage(const std::wstring& text, bool isError) {
  message_ = text;
  messageIsError_ = isError;
  HWND c = GetDlgItem(hwnd_, IDC_MESSAGE);
  if (c) {
    SetWindowTextW(c, message_.c_str());
    InvalidateRect(c, nullptr, TRUE);
  }
}

void MainWindow::updateHotkeyDisplays() {
  for (int i = 0; i < 3; ++i) {
    std::wstring text = (capturing_ == i) ? L"Press keys..." : widen(toString(hotkeyRef(i)));
    SetWindowTextW(GetDlgItem(hwnd_, IDC_HK_FIELD0 + i), text.c_str());
  }
}

void MainWindow::refreshUi() {
  const AppState s = state_.state();
  const bool idle = (s == AppState::Idle);

  SetWindowTextW(GetDlgItem(hwnd_, IDC_RECORD), s == AppState::Recording ? L"Stop Recording" : L"Record");
  EnableWindow(GetDlgItem(hwnd_, IDC_RECORD), s != AppState::Playing);
  SetWindowTextW(GetDlgItem(hwnd_, IDC_PLAYBACK), s == AppState::Playing ? L"Stop Playback" : L"Playback");
  EnableWindow(GetDlgItem(hwnd_, IDC_PLAYBACK), s != AppState::Recording);

  for (int i = 0; i < 3; ++i) {
    HWND b = GetDlgItem(hwnd_, IDC_HK_BTN0 + i);
    SetWindowTextW(b, capturing_ == i ? L"Cancel" : L"Change");
    EnableWindow(b, idle);
  }
  EnableWindow(GetDlgItem(hwnd_, IDC_REPEAT_EDIT), idle && !repeat_.unlimited());
  EnableWindow(GetDlgItem(hwnd_, IDC_UNLIMITED), idle);

  SetWindowTextW(GetDlgItem(hwnd_, IDC_STATUS), widen(toString(s)).c_str());
  updateHotkeyDisplays();

  for (int id : {IDC_RECORD, IDC_PLAYBACK, IDC_UNLIMITED, IDC_REPEAT_EDIT, IDC_STATUS}) {
    InvalidateRect(GetDlgItem(hwnd_, id), nullptr, TRUE);
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

// ------------------------------------------------------------------ commands

void MainWindow::onCommand(int id, int code) {
  switch (id) {
    case IDC_RECORD:
      if (code == BN_CLICKED) toggleRecording(false);
      break;
    case IDC_PLAYBACK:
      if (code == BN_CLICKED) {
        if (state_.state() == AppState::Playing) stopPlayback();
        else startPlayback();
      }
      break;
    case IDC_HK_BTN0:
    case IDC_HK_BTN0 + 1:
    case IDC_HK_BTN0 + 2:
      if (code == BN_CLICKED) {
        int index = id - IDC_HK_BTN0;
        if (capturing_ == index) cancelCapture();
        else beginCapture(index);
      }
      break;
    case IDC_UNLIMITED:
      if (code == BN_CLICKED) onUnlimitedToggled();
      break;
    case IDC_REPEAT_EDIT:
      if (code == EN_CHANGE && !updatingEdit_) {
        wchar_t buf[32] = L"";
        GetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_EDIT), buf, 32);
        std::string narrow;
        for (const wchar_t* p = buf; *p; ++p) narrow.push_back(*p < 128 ? static_cast<char>(*p) : '?');
        repeat_.setCountText(narrow);  // remembered only if valid
      } else if (code == EN_SETFOCUS) {
        editFocused_ = true;
        InvalidateRect(hwnd_, nullptr, FALSE);
      } else if (code == EN_KILLFOCUS) {
        editFocused_ = false;
        commitRepeatText();
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      break;
    case IDC_DONATE:
      if (code == BN_CLICKED) openDonatePage();
      break;
    case IDOK:      // Enter pressed inside the edit box: commit the value
    case IDCANCEL:
      SetFocus(hwnd_);
      break;
    default:
      break;
  }
}

bool MainWindow::commitRepeatText() {
  HWND edit = GetDlgItem(hwnd_, IDC_REPEAT_EDIT);
  wchar_t buf[32] = L"";
  GetWindowTextW(edit, buf, 32);
  std::string narrow;
  for (const wchar_t* p = buf; *p; ++p) narrow.push_back(*p < 128 ? static_cast<char>(*p) : '?');

  RepeatParseResult r = parseRepeatCount(narrow);
  if (r.ok) {
    repeat_.setCountText(narrow);
    return true;
  }
  updatingEdit_ = true;
  SetWindowTextW(edit, widen(repeat_.countText()).c_str());  // restore last valid value
  updatingEdit_ = false;
  setMessage(widen(r.error) + L" Restored " + widen(repeat_.countText()) + L".", true);
  return false;
}

void MainWindow::onUnlimitedToggled() {
  if (state_.state() != AppState::Idle) return;
  if (!repeat_.unlimited()) {
    // Turning Unlimited ON: make sure the remembered quantity is the valid one.
    commitRepeatText();
    repeat_.setUnlimited(true);
  } else {
    // Turning it OFF: restore the previous quantity into the box.
    repeat_.setUnlimited(false);
    updatingEdit_ = true;
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_EDIT), widen(repeat_.countText()).c_str());
    updatingEdit_ = false;
  }
  refreshUi();
}

void MainWindow::openDonatePage() {
  HINSTANCE r = ShellExecuteW(hwnd_, L"open", kDonateUrl, nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(r) <= 32)
    setMessage(L"Could not open your browser. Copy this link: " + std::wstring(kDonateUrl), true);
}

// ------------------------------------------------------------------ recording / playback

void MainWindow::toggleRecording(bool viaHotkey) {
  switch (state_.state()) {
    case AppState::Idle:
      if (capturing_ >= 0) return;
      startRecording();
      break;
    case AppState::Recording:
      stopRecording(viaHotkey);
      break;
    case AppState::Playing:
      setMessage(L"Stop playback before recording.", true);
      break;
  }
}

void MainWindow::startRecording() {
  if (!state_.beginRecording()) return;
  std::string error;
  if (!recorder_.start(hwnd_, error)) {
    state_.endRecording();
    setMessage(widen(error), true);
    refreshUi();
    return;
  }
  setMessage(L"Recording... press " + widen(toString(settings_.record)) + L" to stop.", false);
  refreshUi();
}

void MainWindow::stopRecording(bool viaHotkey) {
  Macro recorded = recorder_.stop();
  const bool truncated = recorder_.truncated();
  state_.endRecording();

  // The hotkey used to stop recording was itself recorded; remove its keys.
  if (viaHotkey) recorded.removeHeldHotkeyKeys(settings_.record);

  // A recording with only the initial cursor position has nothing in it.
  if (recorded.size() <= 1) {
    setMessage(macro_ ? L"Nothing was recorded. Your previous recording was kept."
                      : L"Nothing was recorded.",
               true);
  } else {
    const size_t count = recorded.size();
    const int64_t duration = recorded.durationMs();
    macro_ = std::make_shared<const Macro>(std::move(recorded));
    std::wstring msg = L"Recorded " + std::to_wstring(count) + L" events (" + formatSeconds(duration) + L").";
    if (truncated) msg += L" Event limit reached; the end was cut off.";
    setMessage(msg, truncated);
  }
  refreshUi();
}

void MainWindow::startPlayback() {
  switch (state_.state()) {
    case AppState::Playing:
      return;  // already playing; only one worker may run
    case AppState::Recording:
      setMessage(L"Stop recording before playing back.", true);
      return;
    case AppState::Idle:
      break;
  }
  if (capturing_ >= 0) return;
  if (!macro_ || macro_->empty()) {
    setMessage(L"Nothing to play yet. Record a macro first.", true);
    return;
  }
  if (!repeat_.unlimited() && !commitRepeatText()) return;  // invalid value was rejected

  if (!state_.beginPlayback()) return;
  std::string error;
  if (!player_.start(hwnd_, kMsgPlaybackDone, macro_, repeat_.playbackRepeats(), playbackSpeedPercent_, error)) {
    state_.endPlayback();
    setMessage(widen(error), true);
    refreshUi();
    return;
  }
  setMessage(L"Playing... press " + widen(toString(settings_.stopPlayback)) + L" to stop.", false);
  refreshUi();
}

void MainWindow::stopPlayback() {
  if (state_.state() != AppState::Playing) return;
  player_.requestStop();
  setMessage(L"Stopping...", false);
}

void MainWindow::onPlaybackDone() {
  player_.join();
  state_.endPlayback();
  switch (player_.result()) {
    case PlaybackResult::Finished: setMessage(L"Playback finished.", false); break;
    case PlaybackResult::Stopped: setMessage(L"Playback stopped.", false); break;
    case PlaybackResult::Failed: setMessage(widen(player_.errorMessage()), true); break;
  }
  refreshUi();
}

// ------------------------------------------------------------------ hotkeys

Hotkey& MainWindow::hotkeyRef(int index) {
  switch (index) {
    case 0: return settings_.record;
    case 1: return settings_.playback;
    default: return settings_.stopPlayback;
  }
}

const wchar_t* MainWindow::hotkeyLabel(int index) const {
  switch (index) {
    case 0: return L"Record / Stop Recording";
    case 1: return L"Playback";
    default: return L"Stop Playback";
  }
}

std::vector<int> MainWindow::registerAllHotkeys() {
  unregisterAllHotkeys();
  std::vector<int> failed;
  for (int i = 0; i < 3; ++i) {
    const Hotkey& hk = hotkeyRef(i);
    registered_[i] =
        RegisterHotKey(hwnd_, i + 1, hk.modifiers | MOD_NOREPEAT, hk.vk) != FALSE;
    if (!registered_[i]) failed.push_back(i);
  }
  return failed;
}

void MainWindow::unregisterAllHotkeys() {
  for (int i = 0; i < 3; ++i) {
    if (registered_[i]) UnregisterHotKey(hwnd_, i + 1);
    registered_[i] = false;
  }
}

void MainWindow::beginCapture(int index) {
  if (state_.state() != AppState::Idle) return;
  if (capturing_ < 0) unregisterAllHotkeys();  // so the current keys can be re-pressed
  capturing_ = index;
  setMessage(std::wstring(L"Press the new shortcut for \"") + hotkeyLabel(index) +
                 L"\". Press Esc to cancel.",
             false);
  refreshUi();
  SetFocus(hwnd_);
}

void MainWindow::cancelCapture() {
  if (capturing_ < 0) return;
  capturing_ = -1;
  std::vector<int> failed = registerAllHotkeys();
  if (failed.empty()) setMessage(L"Hotkey change cancelled.", false);
  else setMessage(L"Hotkey change cancelled, but some hotkeys could not be registered.", true);
  refreshUi();
}

bool MainWindow::consumeCaptureMessage(const MSG& msg) {
  if (msg.hwnd != hwnd_ && !IsChild(hwnd_, msg.hwnd)) return false;
  switch (msg.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      handleCaptureKey(static_cast<UINT>(msg.wParam));
      return true;
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
      return true;  // swallow so nothing beeps or opens the system menu
    default:
      return false;
  }
}

void MainWindow::handleCaptureKey(UINT vk) {
  if (vk == VK_PROCESSKEY || vk == 0 || vk == 0xFF) return;
  if (isModifierKey(vk)) return;  // wait for the main key

  uint32_t mods = 0;
  if (GetKeyState(VK_CONTROL) & 0x8000) mods |= mod::Ctrl;
  if (GetKeyState(VK_MENU) & 0x8000) mods |= mod::Alt;
  if (GetKeyState(VK_SHIFT) & 0x8000) mods |= mod::Shift;
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mods |= mod::Win;

  if (vk == VK_ESCAPE && mods == 0) {
    cancelCapture();
    return;
  }
  tryAssignHotkey(capturing_, Hotkey{mods, vk});
}

bool MainWindow::tryAssignHotkey(int index, const Hotkey& candidate) {
  ValidationResult v = validateHotkey(candidate);
  if (!v.ok) {
    setMessage(widen(v.message) + L" Try again or press Esc.", true);
    return false;
  }
  std::vector<NamedHotkey> others;
  for (int i = 0; i < 3; ++i) {
    if (i != index) others.push_back({kHotkeyNames[i], hotkeyRef(i)});
  }
  ValidationResult c = checkConflict(candidate, others);
  if (!c.ok) {
    setMessage(widen(c.message) + L" Try again or press Esc.", true);
    return false;
  }

  const Hotkey previous = hotkeyRef(index);
  hotkeyRef(index) = candidate;
  capturing_ = -1;
  std::vector<int> failed = registerAllHotkeys();

  if (std::find(failed.begin(), failed.end(), index) != failed.end()) {
    // Windows refused (another program owns it): go back to the old key.
    hotkeyRef(index) = previous;
    registerAllHotkeys();
    setMessage(widen(toString(candidate)) +
                   L" is already used by another program. Kept " + widen(toString(previous)) + L".",
               true);
  } else {
    setMessage(std::wstring(hotkeyLabel(index)) + L" is now " + widen(toString(candidate)) + L".", false);
    saveCurrentSettings();
  }
  refreshUi();
  return true;
}

}  // namespace macrolite
