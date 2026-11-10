#include "WidgetSurfaceNative.h"
#include <utility>

namespace ii::gui::detail {
std::unique_ptr<WidgetSurface> make_native_widget_surface(void* parent, WidgetSurfaceCallbacks callbacks) {
    return std::make_unique<NativeWidgetSurface>(parent, std::move(callbacks));
}
} // namespace ii::gui::detail
