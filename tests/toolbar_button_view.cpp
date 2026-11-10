#include <iiGUI/NativeToolbarButtonView.h>
#include <iiGUI/WidgetState.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
struct Source final : ii::gui::ButtonIconSource {
    std::shared_ptr<const ii::gui::RasterImage> image;
    mutable unsigned calls=0; std::uint64_t version=0;
    std::uint64_t revision() const noexcept override { return version; }
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name,double size) const override {
        ++calls;
        if(name!="home" || size!=18) return ii::core::fail(ii::core::ErrorCode::not_found,"Missing icon");
        return image;
    }
};
}
int main() {
    using namespace ii::gui;
    ii::gui::ToolbarButtonInput input; input.configuration.id = std::string("open"); input.configuration.text = "Open";
    input.configuration.width = 100; input.configuration.accessibility_name = "Open document"; input.icon_name = "home";
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));input.icon_source=source;
    unsigned notifications=0; detail::WidgetFrameHooks hooks; hooks.accessibility_changed=[&]{++notifications;};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); },WidgetAppearance{},hooks);
    auto host = take(tree->content_host({}, "host", {200, 80}));
    std::optional<ii::gui::ToolbarButtonView> button; std::vector<ii::gui::ToolbarButtonId> triggered;
    int clicks = 0; bool reconfigure = false, retire = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = ii::gui::ToolbarButtonView::create(builder, *column, "button", input,
            {.clicked = [&] { ++clicks; if (reconfigure) { auto next = input; next.configuration.id = std::int64_t(7); good(button->set_options(next)); }
                              if (retire) good(host.clear_content()); },
             .triggered = [&](const auto& id) { triggered.push_back(id); }});
        if (!value) return std::unexpected(value.error()); button = *value; return {};
    }));
    auto root = button->root(); auto snapshot = take(root.snapshot());
    const auto before=notifications;good(button->set_options(input));
    require(notifications==before&&source->calls==1,"Unchanged toolbar button re-resolved an icon or emitted accessibility changes");
    ++source->version;++input.icon_revision;good(button->set_options(input));
    require(source->calls==2,"A changed icon source revision was not resolved");
    require(snapshot.size.width == 100 && snapshot.size.height == 22 && take(root.action_options()).accessibility_name == "Open document",
            "Toolbar button lost its size or accessible label");
    auto drawing = take(tree->drawing_snapshot({200, 80})); std::size_t images = drawing.images.size(), texts = drawing.texts.size();
    for (const auto& layer : drawing.overlays) { images += layer.images.size(); texts += layer.texts.size(); }
    require(images == 1 && texts >= 1, "Toolbar button did not draw its icon and label");
    good(root.activate()); require(clicks == 1 && triggered == std::vector<ii::gui::ToolbarButtonId>{std::string("open")},
            "Toolbar button did not deliver its own action and identity");
    auto natural = input; natural.configuration.width = 0; natural.configuration.text.clear(); good(button->set_options(natural));
    require(take(root.snapshot()).size.width == 22, "An icon-only toolbar button lost its natural width");
    good(button->set_options(input));
    bool overflow = false;
    try { (void)ii::gui::toolbar_button_id(std::uint64_t(std::numeric_limits<std::int64_t>::max()) + 1); }
    catch (const std::out_of_range&) { overflow = true; }
    require(overflow, "An out-of-range toolbar identity was silently truncated");
    auto next = input; next.icon_name = "missing-icon";
    require(!button->set_options(next) && take(button->options()) == input, "An invalid icon replaced a working toolbar button");
    next = input; next.configuration.visible = false; good(button->set_options(next));
    require(!take(root.interaction()).visible && !root.activate(), "A hidden toolbar button remained actionable");
    next.configuration.visible = true; next.configuration.enabled = false; good(button->set_options(next)); good(root.activate());
    require(clicks == 1 && triggered.size() == 1, "A disabled toolbar button delivered its callback");
    next.configuration.enabled = true; next.configuration.tone = ii::gui::ButtonTone::disabled; good(button->set_options(next)); good(root.activate());
    require(clicks == 1, "A disabled toolbar tone delivered its callback");
    good(button->set_options(input)); bool rejected = false;
    std::thread worker([&] { auto result = button->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Toolbar button accepted an update from a foreign thread");
    reconfigure = true; good(root.activate()); reconfigure = false;
    require(clicks == 2 && triggered.size() == 1 && std::get<std::int64_t>(take(button->options()).configuration.id) == 7,
            "Reconfiguration during a click delivered a stale identity");
    good(root.activate()); require(clicks == 3 && std::get<std::int64_t>(triggered.back()) == 7,
            "The updated toolbar identity was not used for the next action");
    retire = true; good(root.activate()); require(clicks == 4 && triggered.size() == 2 && !button->set_options(input),
            "A retiring toolbar button delivered a stale callback or accepted an update");
    tree->close();
}
