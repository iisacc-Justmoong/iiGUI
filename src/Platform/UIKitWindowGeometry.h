#pragma once

#include <iiGUI/Host.h>

namespace ii::gui::detail {
// The operating system owns a scene's size. A view's initial dimensions are
// advisory, while subsequent explicit size updates retain the host's ordinary
// unsupported result. In-scene overlay windows keep their existing geometry.
class UIKitWindowGeometry final {
public:
    explicit UIKitWindowGeometry(bool managed) noexcept : managed_(managed) {}
    [[nodiscard]] bool managed() const noexcept { return managed_; }
    [[nodiscard]] WindowGeometry prepare(const WindowGeometry& input) const {
        auto result = input;
        if (managed_ && initial_) {
            result.width.reset();
            result.height.reset();
            result.minimum_width.reset();
            result.minimum_height.reset();
        }
        return result;
    }
    void finish_initial() noexcept { initial_ = false; }
private:
    bool managed_;
    bool initial_ = true;
};
} // namespace ii::gui::detail
