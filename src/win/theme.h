// Colour palette (from the project brief).
#pragma once
#include <windows.h>

namespace macrolite {
namespace theme {

inline const COLORREF kBackground = RGB(0x30, 0x38, 0x41);  // #303841
inline const COLORREF kText = RGB(0xD8, 0xDE, 0xE9);        // #D8DEE9
inline const COLORREF kAccent = RGB(0x84, 0x99, 0xB3);      // #8499B3
inline const COLORREF kLightAccent = RGB(0xA0, 0xB9, 0xD9); // #A0B9D9
inline const COLORREF kSurface = RGB(0x41, 0x4B, 0x58);     // #414B58
inline const COLORREF kDark = RGB(0x1B, 0x1F, 0x24);        // #1B1F24
inline const COLORREF kBlack = RGB(0x00, 0x00, 0x00);       // #000000

// Extra colours for state indicators and disabled text.
inline const COLORREF kMuted = RGB(0x9A, 0xA5, 0xB4);
inline const COLORREF kDisabledText = RGB(0x7B, 0x86, 0x95);
inline const COLORREF kRecording = RGB(0xE0, 0x6C, 0x75);
inline const COLORREF kPlaying = RGB(0x98, 0xC3, 0x79);

}  // namespace theme
}  // namespace macrolite
