#include <iiGUI/WebSurfaceSize.h>
#include <iiGUI/NativeEventListenerView.h>
#include <iiGUI/WidgetState.h>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using ii::gui::detail::WebSurfaceSize;
using ii::gui::detail::classify_web_surface_size;
static void require(bool value) { if (!value) throw std::runtime_error("web surface size policy"); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
static void responsive_layout() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    std::optional<NativeEventListener> listener;
    std::vector<Rect> changes;
    auto result = tree->construct([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "root", {.alignment=StackAlignment::stretch});
        if (!stack) return std::unexpected(stack.error());
        auto created = NativeEventListener::create(builder, *stack, "viewport", {0,40},
            {.resized=[&](Rect bounds) { changes.push_back(bounds); }});
        if (!created) return std::unexpected(created.error());
        listener = *created;
        auto label = builder.label(listener->root(), "content", {.text="Responsive"});
        if (!label) return std::unexpected(label.error());
        return {};
    });
    require(bool(result));
    require(bool(tree->activate_paint_view()));
    (void)take(tree->drawing_snapshot({320,480}));
    const auto narrow = take(listener->root().snapshot()).bounds;
    (void)take(tree->drawing_snapshot({800,600}));
    const auto wide = take(listener->root().snapshot()).bounds;
    if (narrow.width != 320 || narrow.height != 40)
        throw std::runtime_error("narrow viewport layout: " + std::to_string(narrow.width) + "," + std::to_string(narrow.height));
    if (wide.width != 800 || wide.height != 40)
        throw std::runtime_error("wide viewport layout: " + std::to_string(wide.width) + "," + std::to_string(wide.height));
    if (changes.size() < 2 || changes.back().width != 800 || changes.back().height != 40)
        throw std::runtime_error("missing responsive layout callback");
    const auto count = changes.size();
    (void)take(tree->drawing_snapshot({800,600}));
    require(changes.size() == count);
    tree->close();
}
int main() {
    require(classify_web_surface_size({640,480}, 2) == WebSurfaceSize::drawable);
    require(classify_web_surface_size({0,480}, 1) == WebSurfaceSize::suspended);
    require(classify_web_surface_size({640,0}, 1) == WebSurfaceSize::suspended);
    require(classify_web_surface_size({0,0}, 1) == WebSurfaceSize::suspended);
    require(classify_web_surface_size({-1,0}, 1) == WebSurfaceSize::invalid);
    require(classify_web_surface_size({0,32768}, 1) == WebSurfaceSize::invalid);
    require(classify_web_surface_size({640,480}, 0) == WebSurfaceSize::invalid);
    require(classify_web_surface_size({640,480}, 9) == WebSurfaceSize::invalid);
    require(classify_web_surface_size({std::numeric_limits<double>::quiet_NaN(),0}, 1) == WebSurfaceSize::invalid);
    ii::gui::detail::WebSurfaceVisibility visibility;
    const ii::gui::Size original{640,480};
    require(visibility.visible(false) && !visibility.visible(true));
    auto same = visibility.update(original, 1, original, 1);
    require(!same.changed && !same.resumed);
    auto hidden = visibility.update({0,480}, 1, original, 1);
    require(hidden.size == WebSurfaceSize::suspended && !hidden.changed && !visibility.visible(false));
    auto invalid = visibility.update({-1,480}, 1, original, 1);
    require(invalid.size == WebSurfaceSize::invalid && !visibility.visible(false));
    auto restored = visibility.update(original, 1, original, 1);
    require(restored.resumed && !restored.changed && visibility.visible(false) && !visibility.visible(true));
    auto resized = visibility.update({800,600}, 2, original, 1);
    require(resized.changed && !resized.resumed);
    responsive_layout();
}
