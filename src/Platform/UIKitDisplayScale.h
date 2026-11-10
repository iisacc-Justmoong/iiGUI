#pragma once
#include <cmath>

namespace ii::gui::detail {
// UIKit may send the first trait event before a view is attached to a screen.
// Ignore unavailable scales and duplicate traits; repaint only a real change.
inline bool uikit_display_scale_changed(double previous, double current) noexcept {
    return std::isfinite(current) && current > 0 && previous != current;
}
}
