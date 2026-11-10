#pragma once

using Window = unsigned long;
struct XMapEvent { int type; Window event; Window window; };
struct XUnmapEvent { int type; Window event; Window window; };
union XEvent { int type; XMapEvent xmap; XUnmapEvent xunmap; };

inline constexpr int MapNotify = 19;
inline constexpr int UnmapNotify = 18;
inline constexpr int FocusOut = 10;
inline constexpr int VisibilityNotify = 15;
