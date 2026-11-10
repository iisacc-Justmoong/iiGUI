#include <iiGUI/NativeButtonView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
struct Source final : ii::gui::ButtonIconSource {
    std::shared_ptr<const ii::gui::RasterImage> image;
    mutable unsigned calls = 0;
    std::uint64_t current_revision = 0;
    bool fail = false;
    std::uint64_t revision() const noexcept override { return current_revision; }
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name, double size) const override {
        ++calls;
        if (fail) return ii::core::fail(ii::core::ErrorCode::failed, "Icon update failed");
        if (name != "icon" || size != 16) return ii::core::fail(ii::core::ErrorCode::not_found, "Icon unavailable");
        return image;
    }
};
}
int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 100}));
    auto source = std::make_shared<Source>();
    source->image = take(rgba_image(2, 2, std::vector<std::uint8_t>(16, 255)));
    ButtonInput input;
    input.configuration.text = "Save";
    input.configuration.width = 100; input.configuration.height = 30;
    input.configuration.icon_mode = true; input.configuration.icon_size = 16;
    input.configuration.motion_enabled = false;
    input.configuration.palette.primary = {.2, .4, .6, 1};
    input.configuration.palette.text = {1, 1, 1, .9};
    input.icon_name = "icon"; input.icon_source = source;
    std::vector<std::string> events;
    std::optional<NativeButton> button;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.absolute_stack({}, "root", {240, 100});
        if (!root) return std::unexpected(root.error());
        auto created = NativeButton::create(builder, *root, "button", input, [&] { events.emplace_back("clicked"); });
        if (!created) return std::unexpected(created.error());
        button = *created;
        return button->root().set_frame({20, 20, 100, 30});
    }));
    require(source->calls == 1 && take(button->options()).icon_name == "icon", "Application icon was not resolved");
    auto frame = take(tree->drawing_snapshot({240, 100}));
    bool has_panel = !frame.panels.empty(), has_icon = !frame.images.empty();
    for (const auto& overlay : frame.overlays) {
        has_panel = has_panel || !overlay.panels.empty();
        has_icon = has_icon || !overlay.images.empty();
    }
    require(has_panel && has_icon, "Button did not draw its surface and icon");
    auto updated = input; updated.configuration.text = "Save again";
    good(button->set_options(updated));
    require(source->calls == 1, "An unchanged icon was resolved again");
    ++source->current_revision; ++updated.icon_revision;
    good(button->set_options(updated));
    require(source->calls == 2, "An icon revision did not trigger resolution");
    source->fail = true; ++source->current_revision; ++updated.icon_revision;
    require(!button->set_options(updated) && take(button->options()).configuration.text == "Save again",
            "A failed icon update replaced retained button state");
    source->fail = false; ++source->current_revision;
    good(button->set_options(input));
    good(button->set_events({.pressed = [&] { events.emplace_back("pressed"); },
                             .released = [&] { events.emplace_back("released"); },
                             .canceled = [&] { events.emplace_back("canceled"); }}));
    good(button->set_method([&](const ButtonEvent& event) {
        require(event.trigger == "clicked" && event.enabled && event.effective_enabled && event.tone == ButtonTone::primary,
                "Button method payload changed");
        events.emplace_back("method");
    }));
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = 30, .y = 30}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = 30, .y = 30}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"},
            "Button input and method ordering changed");
    auto invalid = input; invalid.configuration.height = std::numeric_limits<double>::quiet_NaN();
    require(!button->set_options(invalid) && take(button->options()).configuration.height == 30,
            "Invalid update replaced retained button options");
    invalid = input; invalid.timing.press = std::chrono::milliseconds{-1};
    require(!button->set_options(invalid) && take(button->options()).timing == input.timing,
            "Invalid animation timing replaced retained button options");
    invalid = input; invalid.icon_name = std::string("i\0x", 3);
    require(!button->set_options(invalid) && take(button->options()).icon_name == "icon",
            "Invalid icon name replaced retained button options");
    require(!button->set_methods(std::vector<ButtonMethod>(257, [](const auto&) {})),
            "Unbounded method list was accepted");
    bool wrong_thread = false;
    std::thread worker([&] { auto result = button->options(); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "Button accepted a foreign owner thread");
    auto tone = button->weak_borderless_tone_port(); good(tone.apply(true));
    require(take(button->options()).configuration.tone == ButtonTone::primary,
            "Temporary presentation replaced authored tone");
    good(tone.apply(false));
    events.clear();
    good(button->set_events({.released = [&] { events.emplace_back("retired"); good(host.clear_content()); }}));
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = 30, .y = 30}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = 30, .y = 30}));
    require(events == std::vector<std::string>{"retired"}, "Retired button delivered stale methods");
    require(!button->options() && !tone.apply(true), "Retired button retained an active port");
    tree->close();
}
