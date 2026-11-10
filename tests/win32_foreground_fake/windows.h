#pragma once
#include <cstdint>

using UINT = unsigned int;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
struct WINDOWPOS { UINT flags; };

inline constexpr UINT WM_SHOWWINDOW = 0x0018;
inline constexpr UINT WM_SIZE = 0x0005;
inline constexpr UINT WM_WINDOWPOSCHANGED = 0x0047;
inline constexpr UINT WM_ACTIVATEAPP = 0x001c;
inline constexpr UINT SIZE_RESTORED = 0;
inline constexpr UINT SIZE_MINIMIZED = 1;
inline constexpr UINT SIZE_MAXIMIZED = 2;
inline constexpr UINT SIZE_MAXSHOW = 3;
inline constexpr UINT SIZE_MAXHIDE = 4;
inline constexpr UINT SW_PARENTCLOSING = 1;
inline constexpr UINT SW_OTHERZOOM = 2;
inline constexpr UINT SW_PARENTOPENING = 3;
inline constexpr UINT SW_OTHERUNZOOM = 4;
inline constexpr UINT SWP_SHOWWINDOW = 0x0040;
inline constexpr UINT SWP_HIDEWINDOW = 0x0080;
