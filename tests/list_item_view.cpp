#include <iiGUI/NativeListItemView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
bool has_image(const ii::gui::CanvasFrame& frame) {
    if (!frame.images.empty()) return true;
    for (const auto& layer : frame.overlays) if (!layer.images.empty()) return true;
    return false;
}
bool contains(const ii::gui::CanvasFrame& frame, std::string_view text) {
    for (const auto& run : frame.texts) if (run.text == text) return true;
    for (const auto& layer : frame.overlays) for (const auto& run : layer.texts) if (run.text == text) return true;
    return false;
}
}
int main() {
    using namespace ii::gui;
    ListItemDrawing drawing;const auto via_port=ListItemDrawingPort::action(drawing);
    require(via_port.text==list_item_action(drawing).text,"list item drawing port changed action projection");
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400,120}));
    ListItemInput input; input.configuration.variant = ListRowVariant::navigation;
    input.configuration.label = "Camera"; input.configuration.description = "Shared camera";
    std::optional<NativeListItem> item; unsigned activations = 0; bool remove_on_click = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = NativeListItem::create(builder, *column, "item", input, {.activated=[&] {
            ++activations; if (remove_on_click) good(host.clear_content());
        }});
        if (!value) return std::unexpected(value.error()); item = *value; return {};
    }));
    require(take(item->root().snapshot()).size.width == 280 && take(item->root().snapshot()).size.height == 44 &&
            take(item->root().action_options()).accessibility_name == "Camera" &&
            take(item->root().action_options()).accessibility_description == "Shared camera",
            "Standalone list item lost its dimensions or accessible text");
    const auto frame = take(tree->drawing_snapshot({400,120}));
    require(contains(frame,"Camera") && contains(frame,"Shared camera"), "Standalone list item omitted its text");
    good(item->root().activate()); require(activations == 1, "Standalone list item activation was lost");
    auto next = input; next.configuration.label = "Light"; next.configuration.width = 315;
    next.configuration.selected = true; good(item->set_options(next));
    require(take(item->root().action_options()).accessibility_name == "Light" &&
            take(item->root().snapshot()).size.width == 315, "Retained list item did not update");
    auto invalid = next; invalid.configuration.width = -1;
    require(!item->set_options(invalid) && take(item->options()) == next, "Invalid item update replaced the retained state");
    invalid = next; invalid.configuration.icon_name = "missing-icon";
    require(!item->set_options(invalid) && take(item->options()) == next, "Invalid icon replaced the retained item");
    next.icon = take(rgba_image(1, 1, {255, 0, 0, 255})); good(item->set_options(next));
    require(has_image(take(tree->drawing_snapshot({400,120}))), "Provided list icon was not drawn");
    next.configuration.enabled = false; good(item->set_options(next)); good(item->root().activate());
    require(activations == 1, "Disabled list item activated");
    bool wrong = false; std::thread worker([&] { auto result = item->set_options(input);
        wrong = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong, "List item accepted a foreign-thread update");
    next.configuration.enabled = true; good(item->set_options(next)); remove_on_click = true;
    good(item->root().activate()); require(activations == 2 && !item->set_options(input), "Retired list item accepted an update");
    tree->close();
}
