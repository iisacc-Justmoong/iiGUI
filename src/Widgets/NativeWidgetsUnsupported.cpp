#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurface.h>
#include <stdexcept>
namespace ii::gui::detail {
struct WidgetSurface::Impl {};
WidgetSurface::WidgetSurface(void*,WidgetSurfaceCallbacks) { throw std::runtime_error("Self-rendered widget OS input adapter currently requires macOS"); }
WidgetSurface::~WidgetSurface()=default;
void WidgetSurface::invalidate() {}
void WidgetSurface::accessibility_changed() {}
void WidgetSurface::accessibility_value_changed(std::string_view) {}
void WidgetSurface::set_frame_paced(bool) {}
void WidgetSurface::flush() { throw std::runtime_error("Widget surface unavailable"); }
Size WidgetSurface::size() const { return {}; }
double WidgetSurface::backing_scale() const { return 1; }
std::string WidgetSurface::resolve_font_family(std::string_view) const { return {}; }
GraphicsInfo WidgetSurface::info() { throw std::runtime_error("Widget surface unavailable"); }
PixelBuffer WidgetSurface::capture() { throw std::runtime_error("Widget surface unavailable"); }
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
