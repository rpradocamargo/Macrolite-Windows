// Windows virtual-key codes used by the platform-independent core.
// They are duplicated here (instead of including <windows.h>) so the core
// logic can be compiled and unit-tested without any Windows headers.
#pragma once
#include <cstdint>

namespace macrolite {
namespace vkc {
constexpr uint32_t Back = 0x08;
constexpr uint32_t Tab = 0x09;
constexpr uint32_t Return = 0x0D;
constexpr uint32_t Shift = 0x10;
constexpr uint32_t Control = 0x11;
constexpr uint32_t Menu = 0x12;  // Alt
constexpr uint32_t Pause = 0x13;
constexpr uint32_t Escape = 0x1B;
constexpr uint32_t Space = 0x20;
constexpr uint32_t Delete = 0x2E;
constexpr uint32_t LWin = 0x5B;
constexpr uint32_t RWin = 0x5C;
constexpr uint32_t F1 = 0x70;
constexpr uint32_t F4 = 0x73;
constexpr uint32_t F24 = 0x87;
constexpr uint32_t LShift = 0xA0;
constexpr uint32_t RShift = 0xA1;
constexpr uint32_t LControl = 0xA2;
constexpr uint32_t RControl = 0xA3;
constexpr uint32_t LMenu = 0xA4;
constexpr uint32_t RMenu = 0xA5;
}  // namespace vkc
}  // namespace macrolite
