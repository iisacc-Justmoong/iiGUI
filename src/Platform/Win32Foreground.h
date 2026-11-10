#pragma once

#include <windows.h>

namespace ii::gui::detail {

// Focus and occlusion are intentionally separate from a window's shown and
// minimized state. WM_SHOWWINDOW arrives before the native visibility changes.
class Win32WindowVisibility final {
public:
    void seed(bool shown, bool minimized) noexcept { shown_ = shown; minimized_ = minimized; }
    [[nodiscard]] bool visible() const noexcept { return shown_ && !minimized_; }
    bool update(UINT message, WPARAM value, LPARAM detail) noexcept {
        const bool previous = visible();
        switch (message) {
        case WM_SHOWWINDOW:
            if (detail != SW_OTHERZOOM && detail != SW_OTHERUNZOOM) shown_ = value != 0;
            break;
        case WM_SIZE:
            if (value == SIZE_MINIMIZED) minimized_ = true;
            else if (value == SIZE_RESTORED || value == SIZE_MAXIMIZED) minimized_ = false;
            break;
        case WM_WINDOWPOSCHANGED:
            if (const auto* position = reinterpret_cast<const WINDOWPOS*>(detail)) {
                const bool show = (position->flags & SWP_SHOWWINDOW) != 0;
                const bool hide = (position->flags & SWP_HIDEWINDOW) != 0;
                if (show != hide) shown_ = show;
            }
            break;
        default:
            break;
        }
        return previous != visible();
    }
private:
    bool shown_ = false, minimized_ = false;
};
} // namespace ii::gui::detail
