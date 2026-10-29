#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); } }
}

int main() {
    namespace gui = ii::gui;
    auto tree = std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 300}));
    gui::Widget popup, viewport;
    bool fail = false, unstable = false, checked_reentry = false;
    std::vector<gui::PopoverLayoutMeasure> observed;
    gui::PopoverOptions options;
    options.placement = gui::PopoverPlacement::centered; options.padding = 0;
    options.adaptive_layout = gui::PopoverAdaptiveLayout{.viewport_id = "viewport",
        .layout = [&](const gui::PopoverLayoutMeasure& measure) -> ii::core::Result<gui::Rect> {
            observed.push_back(measure);
            if (!checked_reentry) {
                auto reentrant = popup.snapshot();
                require(!reentrant && reentrant.error().code == ii::core::ErrorCode::busy,
                        "Adaptive layout allowed reentrant UI access");
                auto retirement = host.clear_content();
                require(!retirement && retirement.error().code == ii::core::ErrorCode::busy,
                        "Adaptive layout allowed a parent to retire its active domain");
                checked_reentry = true;
            }
            if (fail) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Layout declined");
            const auto width = std::min(200., measure.viewport.width) + (unstable && measure.content.height > 0 ? 1 : 0);
            return gui::Rect{0, float(measure.viewport.height - 80), float(width), 80};
        }};
    good(host.set_content([&](gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
        auto value = builder.popover(*root, "popup", options); if (!value) return std::unexpected(value.error()); popup = *value;
        auto content = builder.vstack(popup, "content", {.alignment = gui::StackAlignment::stretch});
        if (!content) return std::unexpected(content.error());
        auto label = builder.label(*content, "header", {.text = "Header"}); if (!label) return std::unexpected(label.error());
        auto scroll = builder.scroll_view(*content, "viewport", {}); if (!scroll) return std::unexpected(scroll.error()); viewport = *scroll;
        if (auto flex = viewport.set_flex(1); !flex) return flex;
        auto body = builder.vstack(viewport, "body"); if (!body) return std::unexpected(body.error());
        auto text = builder.label(*body, "text", {.text = "Natural scroll content"}); if (!text) return std::unexpected(text.error());
        return {};
    }));
    (void)take(tree->drawing_snapshot({400, 300})); good(popup.open_at(0, 0));
    auto bounds = take(popup.snapshot()).bounds;
    require(checked_reentry && bounds == gui::Rect{0, 220, 200, 80} &&
            std::ranges::any_of(observed, [](const auto& value) { return value.content.height > 0 && value.viewport_content_height > 0; }),
            "Adaptive layout did not use measured content and viewport");
    (void)take(tree->drawing_snapshot({120, 300}));
    bounds = take(popup.snapshot()).bounds;
    require(bounds.width == 120 && bounds.y == 220, "Adaptive popup did not recompute for the viewport");
    fail = true; good(popup.set_popover_options(options));
    auto rejected = popup.snapshot();
    require(!rejected && rejected.error().code == ii::core::ErrorCode::invalid_argument,
            "Adaptive layout failure was silently ignored");
    fail = false; good(popup.set_popover_options(options));
    require(bool(popup.snapshot()), "Adaptive layout did not recover");
    unstable = true; good(popup.set_popover_options(options));
    require(!popup.snapshot(), "Width-dependent layout changed its measured width after measurement");
    unstable = false; good(popup.set_popover_options(options));
    require(bool(popup.snapshot()), "Adaptive layout did not recover from unstable width");
    good(popup.close_popup()); fail = true; good(popup.set_popover_options(options));
    require(!popup.open_at(0, 0) && !take(popup.snapshot()).opened,
            "A rejected initial adaptive layout published the popup");
    fail = false; good(popup.set_popover_options(options)); good(popup.open_at(0, 0)); good(popup.close_popup());
    good(host.clear_content()); tree->close();
}
