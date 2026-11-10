#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <stdexcept>
namespace ii::gui::detail {
struct NativeWidgetSurface::Impl {};
NativeWidgetSurface::NativeWidgetSurface(void*,WidgetSurfaceCallbacks) { throw std::runtime_error("Self-rendered widget OS input adapter currently requires macOS"); }
NativeWidgetSurface::~NativeWidgetSurface()=default;
void NativeWidgetSurface::invalidate() {}
void NativeWidgetSurface::accessibility_changed() {}
void NativeWidgetSurface::accessibility_value_changed(std::string_view) {}
void NativeWidgetSurface::set_frame_paced(bool) {}
void NativeWidgetSurface::flush() { throw std::runtime_error("Widget surface unavailable"); }
Size NativeWidgetSurface::size() const { return {}; }
double NativeWidgetSurface::backing_scale() const { return 1; }
std::string NativeWidgetSurface::resolve_font_family(std::string_view) const { return {}; }
GraphicsInfo NativeWidgetSurface::info() { throw std::runtime_error("Widget surface unavailable"); }
PixelBuffer NativeWidgetSurface::capture() { throw std::runtime_error("Widget surface unavailable"); }
bool NativeWidgetSurface::native_event(const void*) { return false; }
#if defined(_WIN32)
std::optional<std::intptr_t> NativeWidgetSurface::native_result(const void*) { return {}; }
#endif
}
namespace ii::gui {
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Native mounting requires the host thread");
    return core::fail(core::ErrorCode::unsupported, "Native widget mounting currently requires macOS/AppKit");
}
core::Result<void> Host::unmount_widgets() {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Native mounting requires the host thread");
    return core::fail(core::ErrorCode::unsupported, "Native widget mounting currently requires macOS/AppKit");
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance&) {
    return mount_widgets(std::move(factory));
}
core::Result<void> Host::unmount_widgets(const WidgetMount&) { return unmount_widgets(); }
}
