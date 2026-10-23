#pragma once
#include "NativeWidgets.h"

namespace ii::gui::detail {
#if defined(__linux__)
struct X11WidgetParent final {void* display=nullptr;std::uintptr_t window=0;std::function<double()> scale;};
#endif
// One OS input/presentation surface for an entire C++ widget tree. No OS
// control, layout engine, hit test or control action is used for any widget.
struct WidgetSurfaceCallbacks final {
    std::function<CanvasFrame(Size)> frame;
    std::function<void(WidgetInput)> input;
    std::function<std::vector<WidgetSnapshot>()> accessibility;
    std::function<WidgetSnapshot(std::string_view)> lookup;
    std::function<void(std::exception_ptr)> failure;
    Renderer renderer=Renderer::automatic;
};
class WidgetSurface final {
public:
    WidgetSurface(void* parent, WidgetSurfaceCallbacks);
    ~WidgetSurface();
    WidgetSurface(const WidgetSurface&)=delete;
    WidgetSurface& operator=(const WidgetSurface&)=delete;
    void invalidate();
    void accessibility_changed();
    void accessibility_value_changed(std::string_view);
    void set_frame_paced(bool);
    void flush();
    Size size() const;
    double backing_scale() const;
    std::string resolve_font_family(std::string_view) const;
    GraphicsInfo info();
    PixelBuffer capture();
    bool native_event(const void*);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
