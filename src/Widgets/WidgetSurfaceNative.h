#pragma once
#include "WidgetSurface.h"

namespace ii::gui::detail {
class NativeCanvas;
// Host-side OS adapter. This type must be linked only by the host package.
class NativeWidgetSurface final : public WidgetSurface {
public:
    NativeWidgetSurface(void*, WidgetSurfaceCallbacks);
#if defined(_WIN32)
    // The window is borrowed; invalidation schedules the owner's iiGUI pump
    // instead of claiming the foreign window's paint cycle.
    NativeWidgetSurface(void*, WidgetSurfaceCallbacks, std::shared_ptr<NativeCanvas>, std::function<void()> invalidation);
#endif
    ~NativeWidgetSurface() override;
    void invalidate() override;
    void accessibility_changed() override;
    void accessibility_value_changed(std::string_view) override;
#if defined(__APPLE__) && TARGET_OS_IPHONE
    runtime::Task begin_text_input_change(bool) override;
    bool reduced_motion() const override;
#endif
    void set_frame_paced(bool) override;
    void flush() override;
    Size size() const override;
    double backing_scale() const override;
    std::string resolve_font_family(std::string_view) const override;
    GraphicsInfo info() override;
    PixelBuffer capture() override;
    bool native_event(const void*) override;
#if defined(_WIN32) || (defined(__APPLE__) && !TARGET_OS_IPHONE) || (defined(__linux__) && !defined(__ANDROID__))
    void set_native_pointer_hidden(bool) override;
#endif
#if defined(_WIN32)
    std::optional<std::intptr_t> native_result(const void*) override;
#endif
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
std::unique_ptr<WidgetSurface> make_native_widget_surface(void*, WidgetSurfaceCallbacks);
} // namespace ii::gui::detail
