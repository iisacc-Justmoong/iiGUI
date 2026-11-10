#include <iiGUI/NativeAlertButtonView.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
void callback_replacement() {
    using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"callbacks",{180,100}));std::optional<AlertButtonView> button;
    std::vector<std::string> events;bool replace=true;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        button=take(AlertButtonView::create(builder,{},"alert"));return {};
    }));
    AlertButtonCallbacks next;
    next.method=[&](const auto&){events.emplace_back("new method");};
    next.methods={[&](const auto&){events.emplace_back("new list");}};
    next.clicked=[&]{events.emplace_back("new click");};
    AlertButtonCallbacks original;
    original.method=[&](const auto&){events.emplace_back("old method");if(replace){replace=false;good(button->set_callbacks(next));}};
    original.methods={[&](const auto&){events.emplace_back("old list");}};
    original.clicked=[&]{events.emplace_back("old click");};
    good(button->set_callbacks(original));good(button->root().activate());
    require(events==std::vector<std::string>{"old method","old list","new click"},
            "AlertButton replaced its active method snapshot or lost the current click callback");
    events.clear();good(button->root().activate());
    require(events==std::vector<std::string>{"new method","new list","new click"},"AlertButton callback replacement was lost");
    auto invalid=next;invalid.methods.assign(257,[](const auto&){});require(!button->set_callbacks(invalid),"AlertButton accepted too many methods");
    events.clear();good(button->root().activate());
    require(events==std::vector<std::string>{"new method","new list","new click"},"Invalid AlertButton callbacks replaced valid ones");
    good(host.clear_content());tree->close();
}
}

int main() {
    callback_replacement();
    using namespace ii::gui;
    std::optional<ii::gui::AlertButtonView> button;
    bool reenter = false, retire_on_method = false; unsigned accessibility_updates = 0;
    ii::gui::AlertButtonOptions config;
    config.text = "Cancel"; config.dialog_style = true; config.width = 100; config.height = 56;
    auto latest = config; latest.text = "Newest";
    detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed = [&] { ++accessibility_updates; if (reenter) { reenter = false; good(button->set_options(latest)); } };
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); }, WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "host", {300, 200}));
    std::vector<std::string> events;
    std::string source_id;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "root", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        ii::gui::AlertButtonCallbacks callbacks;
        callbacks.method = [&](const ii::gui::ButtonEvent& event) {
            source_id = take(event.source.snapshot()).id;
            require(event.trigger == "clicked", "AlertButton method received an invalid trigger");
            events.push_back("method");
            if (retire_on_method) good(host.clear_content());
        };
        callbacks.clicked = [&] { events.push_back("clicked"); };
        callbacks.pressed = [&] { events.push_back("pressed"); };
        callbacks.released = [&] { events.push_back("released"); };
        callbacks.canceled = [&] { events.push_back("canceled"); };
        auto created = ii::gui::AlertButtonView::create(builder, *stack, "alert-button", config, std::move(callbacks));
        if (!created) return std::unexpected(created.error()); button = *created; return {};
    }));
    auto root = button->root();
    const auto unchanged_updates = accessibility_updates;
    good(button->set_options(config));
    require(accessibility_updates == unchanged_updates, "Unchanged AlertButton options published an accessibility update");
    const auto frame = take(tree->drawing_snapshot({300, 200}));
    auto snapshot = take(root.snapshot());
    bool border = false;
    const auto inspect = [&](const auto& panels) {
        for (const auto& panel : panels) border |= panel.bounds == snapshot.bounds && panel.stroke_width == 1 && panel.radius == 16;
    };
    inspect(frame.panels); for (const auto& layer : frame.overlays) inspect(layer.panels);
    if (!border || snapshot.accessibility_name != "Cancel") {
        throw std::runtime_error("AlertButton outline=" + std::to_string(border) +
            " accessible=" + snapshot.accessibility_name + " text=" + snapshot.text);
    }
    const auto x = snapshot.bounds.x + snapshot.bounds.width / 2, y = snapshot.bounds.y + snapshot.bounds.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"} && source_id == snapshot.id,
            "AlertButton pointer or method callback order changed");
    events.clear(); good(root.focus());
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    good(tree->input({.kind = WidgetInputKind::key_up, .key = WidgetKey::enter}));
    require(events.empty(), "Default AlertButton unexpectedly accepted Enter");
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::space}));
    good(tree->input({.kind = WidgetInputKind::key_up, .key = WidgetKey::space}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"},
            "AlertButton Space callback order changed");
    events.clear(); auto enter = config; enter.enter_key_activation = true; good(button->set_options(enter));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    good(tree->input({.kind = WidgetInputKind::key_up, .key = WidgetKey::enter}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"},
            "Opted-in AlertButton Enter callback order changed");
    events.clear(); good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::focus_lost}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    require(events == std::vector<std::string>{"pressed", "canceled"},
            "AlertButton did not forward cancellation through the shared button");

    auto disabled = config; disabled.enabled = false; good(button->set_options(disabled));
    events.clear(); good(root.activate()); require(events.empty(), "Disabled AlertButton admitted an action");
    auto invalid = config; invalid.height = 0;
    require(!button->set_options(invalid) && !take(button->options()).enabled,
            "Invalid AlertButton options replaced the last valid state");
    auto older = config; older.text = "Older"; reenter = true;
    good(button->set_options(older));
    require(!reenter && take(button->options()).text == "Newest" && take(root.snapshot()).text == "Newest",
            "An older AlertButton update replaced a reentrant update");
    bool rejected = false;
    std::thread worker([&] { auto result = button->set_options(config); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "AlertButton accepted a foreign-thread update");
    events.clear(); retire_on_method = true; good(root.activate());
    require(events == std::vector<std::string>{"method"} && !root.snapshot() && !button->options(),
            "A retired AlertButton delivered a stale click");
    tree->close();
}
