#include <iiGUI/NativeStepperView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) { if (!value) throw std::runtime_error(value.error().message); return std::move(*value); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}
int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {160, 100}));
    std::optional<NativeStepper> stepper; StepperConfiguration input;
    input.palette.primary = {.2, .4, .6, 1}; input.palette.text = {1, 1, 1, 1};
    std::vector<int> steps; unsigned clicked = 0, canceled = 0;
    auto callback_owner = std::make_shared<int>(1); std::weak_ptr<int> weak_callback = callback_owner;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        StepperCallbacks callbacks;
        callbacks.stepped = [&](int direction) { steps.push_back(direction); };
        callbacks.clicked = [&, keep = callback_owner] { ++clicked; };
        callbacks.canceled = [&] { ++canceled; };
        callbacks.released = [&] { good(stepper->set_options(input)); };
        auto created = NativeStepper::create(builder, {}, "stepper", input, std::move(callbacks));
        if (!created) return std::unexpected(created.error()); stepper = *created; return {};
    }));
    callback_owner.reset();
    auto method_owner = std::make_shared<int>(2); std::weak_ptr<int> weak_method = method_owner;
    good(stepper->set_method([keep = method_owner](const ButtonEvent&) {})); method_owner.reset();
    require(!weak_callback.expired() && !weak_method.expired(), "Mounted stepper lost its callbacks");
    (void)take(tree->drawing_snapshot({160, 100}));
    const auto snapshot = take(stepper->root().snapshot()); const auto bounds = snapshot.bounds;
    require(snapshot.accessibility_semantics && snapshot.accessibility_semantics->role == AccessibilityRole::stepper &&
            snapshot.accessibility_semantics->can_increment && snapshot.accessibility_semantics->can_decrement,
            "Stepper accessibility actions are missing");
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 4, .y = bounds.y + 3}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 4, .y = bounds.y + 3}));
    require(steps == std::vector<int>{1} && clicked == 1, "An identical update interrupted an active gesture");
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 4, .y = bounds.y + 3}));
    good(tree->input({.kind = WidgetInputKind::pointer_cancel, .x = bounds.x + 4, .y = bounds.y + 3}));
    require(canceled == 1 && steps.size() == 1, "Pointer cancellation did not end the active gesture");
    auto invalid = input; invalid.height = 0;
    require(!stepper->set_options(invalid) && take(stepper->options()) == input, "Invalid stepper input replaced the previous state");
    good(host.clear_content());
    require(!stepper->options() && weak_callback.expired() && weak_method.expired(),
            "Retired stepper retained its callbacks or remained accessible");
    tree->close();
}
