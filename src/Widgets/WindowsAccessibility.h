#pragma once

#include "WidgetSurface.h"
#include <cstdint>
#include <memory>
#include <optional>

namespace ii::gui::detail {
struct WindowsAccessibilityState;

// Native peers outlive the surface only as disconnected references. Widget
// access stays on the surface owner thread; clients read immutable snapshots.
class WindowsAccessibility final {
    std::shared_ptr<WindowsAccessibilityState> state_;
    bool uninitialize_=false;
public:
    WindowsAccessibility(void* window,const WidgetSurfaceCallbacks&);
    ~WindowsAccessibility();
    WindowsAccessibility(const WindowsAccessibility&)=delete;
    WindowsAccessibility& operator=(const WindowsAccessibility&)=delete;
    void retire() noexcept;
    void refresh() noexcept;
    void structure_changed() noexcept;
    void value_changed(std::string_view id) noexcept;
    [[nodiscard]] bool input(const void* message) noexcept;
    [[nodiscard]] std::optional<std::intptr_t> result(const void* message) noexcept;
};
} // namespace ii::gui::detail
