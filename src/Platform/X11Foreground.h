#pragma once

#include "WindowForeground.h"
#include <X11/Xlib.h>

namespace ii::gui::detail {

// StructureNotify reports the WM's mapping transitions, not focus or occlusion.
// The first native event supplies the initial state; a map request alone does not.
class X11WindowForeground final {
public:
    bool apply(Window window, const XEvent& event, WindowForeground& application, const void* owner) {
        if (retired_ || !window || !owner) return false;
        bool mapped;
        if (event.type == MapNotify && event.xmap.event == window && event.xmap.window == window) mapped = true;
        else if (event.type == UnmapNotify && event.xunmap.event == window && event.xunmap.window == window) mapped = false;
        else return false;
        if (known_ && mapped_ == mapped) return false;
        if (!known_) application.open_window(owner, mapped);
        else application.set_window_visible(owner, mapped);
        known_ = true;
        mapped_ = mapped;
        return true;
    }

    void retire(WindowForeground& application, const void* owner) noexcept {
        if (retired_) return;
        retired_ = true;
        if (known_) application.remove_window(owner);
    }
private:
    bool known_ = false, mapped_ = false, retired_ = false;
};
} // namespace ii::gui::detail
