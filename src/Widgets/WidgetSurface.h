#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Presentation.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

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
    std::function<core::Result<TextContent>(std::string_view,TextRange)> text_content;
    std::function<core::Result<std::size_t>(std::string_view,double,double)> text_position;
    Renderer renderer=Renderer::automatic;
    std::shared_ptr<PresentationStream> presentations;
    std::function<core::Result<void>(WidgetInput)> text_service;
    std::function<core::Result<TextInputGeometry>(std::string_view,TextRange)> text_geometry;
    std::function<core::Result<void>(std::string_view,const TextInputRevision&,std::string)> accessibility_text;
    // The focused text editor only; avoids materializing unrelated accessibility nodes.
    std::function<std::optional<WidgetSnapshot>()> focused_editor;
    // Synchronous owner-thread probe without accessibility snapshots.
    std::function<bool(double,double)> touch_scrollable;
    // Platform samples enter here; kinetic behavior remains in the shared runtime.
    std::function<void(double)> touch_begin,touch_end;
    std::function<void()> touch_cancel;
};
// Implemented by a host adapter. Widget state only retains this interface;
// it never constructs an operating-system surface itself.
class WidgetSurface {
public:
    virtual ~WidgetSurface() = default;
    virtual void invalidate() = 0;
    virtual void accessibility_changed() = 0;
    virtual void accessibility_value_changed(std::string_view) = 0;
    virtual runtime::Task begin_text_input_change(bool) { return {}; }
    virtual bool reduced_motion() const { return false; }
    virtual void set_frame_paced(bool) = 0;
    virtual void flush() = 0;
    virtual Size size() const = 0;
    virtual double backing_scale() const = 0;
    virtual std::string resolve_font_family(std::string_view) const = 0;
    virtual GraphicsInfo info() = 0;
    virtual PixelBuffer capture() = 0;
    virtual bool native_event(const void*) = 0;
#if defined(_WIN32)
    virtual std::optional<std::intptr_t> native_result(const void*) = 0;
#endif
};
using WidgetSurfaceFactory = std::function<std::unique_ptr<WidgetSurface>(void*, WidgetSurfaceCallbacks)>;
} // namespace ii::gui::detail
