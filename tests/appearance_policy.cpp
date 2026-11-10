#include <iiGUI/WidgetState.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace ii;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
unsigned weight(const std::shared_ptr<gui::detail::WidgetState>& tree, const gui::Widget& widget) {
    const auto frame = take(tree->drawing_snapshot({640, 480}));
    const auto bounds = take(widget.snapshot()).bounds;
    const auto find = [&](const auto& values) -> unsigned {
        for (const auto& value : values) if (value.bounds == bounds && value.text == "Typography sample") return value.font_weight;
        return 0;
    };
    if (auto found = find(frame.texts)) return found;
    for (const auto& layer : frame.overlays) if (auto found = find(layer.texts)) return found;
    throw std::runtime_error("Typography sample was not drawn");
}
}

int main() {
    using namespace ii;
    gui::require_text_weight_rules_abi_v1();
    const auto generic = gui::default_widget_appearance();
    check(generic == gui::lvrs_appearance(), "Compatibility appearance changed");
    const auto defaults = gui::default_text_tokens(), legacy = gui::lvrs_text_tokens();
    check(defaults.size() == legacy.size(), "Compatibility text token count changed");
    for (std::size_t i = 0; i < defaults.size(); ++i)
        check(defaults[i].pixels == legacy[i].pixels && defaults[i].weight == legacy[i].weight && defaults[i].style == legacy[i].style,
              "Compatibility text token changed");

    int geometry_updates = 0;
    gui::detail::WidgetFrameHooks hooks;
    hooks.window_geometry = [&](const gui::WindowGeometry&) -> core::Result<void> { ++geometry_updates; return {}; };
    auto tree = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); }, generic, std::move(hooks));
    gui::Widget implicit, explicit_weight, nested, field;
    good(tree->construct([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        auto root = take(builder.vstack({}, "root", {.alignment = gui::StackAlignment::leading}));
        implicit = take(builder.label(root, "implicit", {.text = "Typography sample", .font_size = 15, .role = gui::TextRole::body}));
        explicit_weight = take(builder.label(root, "explicit", {.text = "Typography sample", .font_size = 15,
            .role = gui::TextRole::body, .font_weight = 700}));
        auto host = take(builder.content_host(root, "nestedHost", {300, 50}));
        good(host.set_content([&](gui::WidgetBuilder& child) -> core::Result<void> {
            auto stack = take(child.vstack({}, "nestedRoot", {.alignment = gui::StackAlignment::leading}));
            nested = take(child.label(stack, "nested", {.text = "Typography sample", .font_size = 15, .role = gui::TextRole::body}));
            return {};
        }));
        field = take(builder.input_field(root, "field", {.text = "Retained"}));
        return {};
    }));
    good(tree->window_properties({.geometry = {.title = "Typography"}, .appearance = generic}));
    good(tree->commit_window_properties()); tree->commit_publication();
    check(weight(tree, implicit) == 600 && weight(tree, nested) == 600 && weight(tree, explicit_weight) == 700,
          "Legacy default text rules changed");
    good(field.focus());
    const auto field_before = take(field.snapshot());
    (void)take(implicit.measure(600));
    const auto expected = take(explicit_weight.measure(600));
    auto appearance = generic;
    appearance.text_weight_rules = std::vector<gui::TextWeightRule>{{15, 400}, {15, 700}};
    good(tree->window_properties({.geometry = {.title = "Typography"}, .appearance = appearance}));
    check(weight(tree, implicit) == 700 && weight(tree, nested) == 700 && weight(tree, explicit_weight) == 700,
          "Explicit text rules did not propagate with last-match and explicit-weight precedence");
    const auto measured = take(implicit.measure(600));
    check(std::abs(measured.width - expected.width) < .01 && std::abs(measured.height - expected.height) < .01,
          "Text rule update kept stale measurement caches");
    const auto field_after = take(field.snapshot());
    check(field_after.id == field_before.id && field_after.text == field_before.text && field_after.focused,
          "Text rule update replaced or unfocused an input");

    const auto before = take(tree->window_properties());
    const auto updates_before = geometry_updates;
    for (auto invalid : {gui::TextWeightRule{std::numeric_limits<double>::quiet_NaN(), 400},
                         gui::TextWeightRule{0, 400}, gui::TextWeightRule{257, 400}, gui::TextWeightRule{15, 450}}) {
        auto candidate = before; candidate.geometry.title = "Rejected"; candidate.appearance->text_weight_rules = std::vector{invalid};
        check(!tree->window_properties(candidate) && take(tree->window_properties()) == before && geometry_updates == updates_before &&
              weight(tree, implicit) == 700, "Invalid text policy partially changed the window");
    }
    auto excessive = before; excessive.appearance->text_weight_rules = std::vector<gui::TextWeightRule>(257, {15, 400});
    check(!tree->window_properties(excessive) && take(tree->window_properties()) == before,
          "An excessive text policy was accepted");
    bool invalid_initial = false;
    try {
        auto bad = generic; bad.text_weight_rules = std::vector<gui::TextWeightRule>{{15, 450}};
        (void)std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); }, std::move(bad));
    } catch (const std::invalid_argument&) { invalid_initial = true; }
    check(invalid_initial, "An invalid initial text policy was accepted");
    appearance.text_weight_rules = std::vector<gui::TextWeightRule>{};
    good(tree->window_properties({.geometry = {.title = "Typography"}, .appearance = appearance}));
    check(weight(tree, implicit) == 500 && weight(tree, nested) == 500 && weight(tree, explicit_weight) == 700,
          "An empty text policy did not restore role defaults");
    appearance.text_weight_rules.reset();
    good(tree->window_properties({.geometry = {.title = "Typography"}, .appearance = appearance}));
    check(weight(tree, implicit) == 600 && weight(tree, nested) == 600,
          "Clearing a text policy did not restore compatibility rules");
    tree->close();
}
