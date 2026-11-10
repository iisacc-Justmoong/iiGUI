#include <iiGUI/ProgressIndicator.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) throw std::runtime_error(result.error().message);
}
}
int main() {
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::optional<ii::gui::ProgressIndicator> indicator;
    ii::gui::ProgressConfiguration config;
    config.motion_enabled = false;
    config.values.current = 25;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "stack");
        if (!stack) return std::unexpected(stack.error());
        auto created = ii::gui::ProgressIndicator::create(builder, *stack, "progress", config, false);
        if (!created) return std::unexpected(created.error());
        indicator = std::move(*created);
        return {};
    }));
    auto widget = indicator->root();
    auto before = take(widget.snapshot());
    require(before.accessibility_semantics && before.accessibility_semantics->role == ii::gui::AccessibilityRole::progress &&
            std::abs(before.value - .25) < 1e-9, "Initial accessibility value differs");
    good(indicator->set_configuration(config));
    auto changed = config;
    changed.values.current = 75;
    changed.style.width = 200;
    good(indicator->set_configuration(changed));
    const auto after = take(widget.snapshot());
    require(after.id == before.id && std::abs(after.value - .75) < 1e-9 &&
            std::abs(after.bounds.width - 200) < 1e-9, "Update replaced or misconfigured the widget");
    auto invalid = changed;
    invalid.values.current = std::numeric_limits<double>::quiet_NaN();
    require(!indicator->set_configuration(invalid), "Non-finite value was accepted");
    require(std::abs(take(widget.snapshot()).value - .75) < 1e-9, "Invalid update changed semantics");
    good(host.clear_content());
    require(!indicator->set_configuration(changed), "Retired widget accepted an unchanged update");
    state->close();
}
