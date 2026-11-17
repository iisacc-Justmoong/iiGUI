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
const gui::TextRun& text(const gui::CanvasFrame& frame, std::string_view value) {
    for (const auto& item : frame.texts) if (item.text == value) return item;
    for (const auto& layer : frame.overlays)
        for (const auto& item : layer.texts) if (item.text == value) return item;
    throw std::runtime_error("Palette test text was not drawn");
}
void check_color(const gui::TextRun& run, gui::WidgetColor expected) {
    const auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
    check(std::abs(run.color.red - linear(expected.red)) < 1e-6 &&
          std::abs(run.color.green - linear(expected.green)) < 1e-6 &&
          std::abs(run.color.blue - linear(expected.blue)) < 1e-6 &&
          std::abs(run.color.alpha - expected.alpha) < 1e-6, "Role color did not reach the rendered text");
}
void palette() {
    using namespace ii;
    const gui::WidgetColor body{.25, .5, .75, .8}, disabled{.8, .3, .1, .4}, explicit_ink{.1, .2, .3, 1}, description{.2,.3,.4,.6};
    auto appearance = gui::default_widget_appearance();
    std::array<gui::WidgetColor, 8> colors{};
    colors.fill({1, 1, 1, 1});
    colors[static_cast<unsigned>(gui::TextRole::body)] = body;
    colors[static_cast<unsigned>(gui::TextRole::description)] = description;
    colors[static_cast<unsigned>(gui::TextRole::disabled)] = disabled;
    appearance.text_colors = colors;
    std::array<gui::TextRoleStyle,8> styles{};
    styles.fill({13,13,500});
    styles[static_cast<unsigned>(gui::TextRole::body)] = {19.5,20,700};
    appearance.text_styles=styles;
    std::optional<gui::TextRoleStyle> painted_style;
    gui::Widget role_probe;
    gui::detail::WidgetFrameHooks hooks;
    hooks.window_geometry = [](const gui::WindowGeometry&) -> core::Result<void> { return {}; };
    auto tree = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); }, appearance, hooks);
    good(tree->construct([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        auto root = take(builder.vstack({}, "root", {.alignment = gui::StackAlignment::leading}));
        take(builder.label(root, "label", {.text = "Palette label", .role = gui::TextRole::body}));
        take(builder.button(root, "button", {.text = "Palette button"}));
        take(builder.check_box(root, "disabled", {.text = "Palette disabled", .enabled = false}));
        take(builder.label(root, "explicit", {.text = "Explicit ink", .role = gui::TextRole::body, .color = explicit_ink}));
        role_probe=take(builder.label(root, "themed", {.text = "Themed label", .font_size=13, .role=gui::TextRole::body,
            .line_height=16, .font_weight=500, .theme_typography_role=gui::TextRole::body,
            .theme_color_role=gui::TextRole::description}));
        take(builder.label(root, "themed-explicit", {.text = "Themed explicit", .color=explicit_ink,
            .theme_typography_role=gui::TextRole::body, .theme_color_role=gui::TextRole::description}));
        auto probe=take(builder.absolute_stack(root,"style-probe",{20,20}));
        good(probe.set_paint_handler([&](const gui::WidgetPaintState& state)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
            painted_style=state.text_styles?std::optional{(*state.text_styles)[static_cast<unsigned>(gui::TextRole::body)]}:std::nullopt;
            return std::make_shared<const gui::WidgetDrawing>();
        }));
        auto child = take(builder.content_host(root, "child", {300, 40}));
        return child.set_content([](gui::WidgetBuilder& nested) -> core::Result<void> {
            auto label = nested.label({}, "child-label", {.text = "Nested palette", .role = gui::TextRole::body});
            return label ? core::Result<void>{} : std::unexpected(label.error());
        });
    }));
    good(tree->window_properties({.geometry = {.title = "Palette"}, .appearance = appearance}));
    good(tree->commit_window_properties()); tree->commit_publication();
    auto frame = take(tree->drawing_snapshot({640, 480}));
    check_color(text(frame, "Palette label"), body);
    check_color(text(frame, "Palette button"), body);
    check_color(text(frame, "Nested palette"), body);
    check_color(text(frame, "Palette disabled"), disabled);
    check_color(text(frame, "Explicit ink"), explicit_ink);
    check_color(text(frame, "Themed label"), description);
    check_color(text(frame, "Themed explicit"), explicit_ink);
    check(text(frame,"Themed label").font_size==19.5f && text(frame,"Themed label").line_height==20 &&
          text(frame,"Themed label").font_weight==700,"Opted-in label ignored the global typography role");
    auto invalid_label=gui::LabelOptions{};invalid_label.theme_color_role=static_cast<gui::TextRole>(8);
    check(!role_probe.set_label_options(invalid_label),"An invalid label theme role was accepted");
    check(painted_style==gui::TextRoleStyle{19.5,20,700},"Role typography did not reach custom paint providers");
    const gui::WidgetColor next_body{.6, .2, .4, .5};
    (*appearance.text_colors)[static_cast<unsigned>(gui::TextRole::body)] = next_body;
    (*appearance.text_styles)[static_cast<unsigned>(gui::TextRole::body)] = {20,21,600};
    good(tree->window_properties({.geometry = {.title = "Palette"}, .appearance = appearance}));
    frame = take(tree->drawing_snapshot({640, 480}));
    check_color(text(frame, "Palette label"), next_body);
    check_color(text(frame, "Nested palette"), next_body);
    check_color(text(frame, "Explicit ink"), explicit_ink);
    check(text(frame,"Themed label").font_size==20 && text(frame,"Themed label").line_height==21 &&
          text(frame,"Themed label").font_weight==600,"Updated role typography did not reach an opted-in label");
    check(painted_style==gui::TextRoleStyle{20,21,600},"Updated role typography did not reach custom paint providers");
    auto invalid = appearance;
    (*invalid.text_colors)[0].red = std::numeric_limits<double>::quiet_NaN();
    check(!tree->window_properties({.geometry = {.title = "Invalid"}, .appearance = invalid}),
          "An invalid role color was accepted");
    auto bad_style=appearance;(*bad_style.text_styles)[0].font_weight=450;
    check(!tree->window_properties({.geometry = {.title = "Invalid"}, .appearance = bad_style}),
          "Invalid role typography was accepted");
    check_color(text(take(tree->drawing_snapshot({640, 480})), "Palette label"), next_body);
    bool rejected=false;
    try { (void)std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); }, invalid); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected, "An invalid initial role color was accepted");
    appearance.text_colors.reset();appearance.text_styles.reset();
    good(tree->window_properties({.geometry = {.title = "Palette"}, .appearance = appearance}));
    check_color(text(take(tree->drawing_snapshot({640, 480})), "Palette label"),
                {1,1,1,appearance.text_alpha[static_cast<unsigned>(gui::TextRole::body)]});
    const auto fallback=take(tree->drawing_snapshot({640,480}));
    check_color(text(fallback,"Themed label"),{1,1,1,appearance.text_alpha[static_cast<unsigned>(gui::TextRole::body)]});
    check(text(fallback,"Themed label").font_size==13 && text(fallback,"Themed label").line_height==16 &&
          text(fallback,"Themed label").font_weight==500,"Opted-in label did not retain its fallback style");
    tree->close();
}
}

int main() {
    using namespace ii;
    palette();
    gui::require_text_weight_rules_abi_v1();
    const auto generic = gui::default_widget_appearance();
    const auto legacy = gui::lvrs_appearance();
    check(generic.window.red==generic.window.green&&generic.window.green==generic.window.blue,
          "Generic window must not incorporate the default accent");
    check(generic.primary==legacy.primary,"Neutral window changed the compatibility accent");
    const auto defaults = gui::default_text_tokens(), legacy_text = gui::lvrs_text_tokens();
    check(defaults.size() == legacy_text.size(), "Compatibility text token count changed");
    for (std::size_t i = 0; i < defaults.size(); ++i)
        check(defaults[i].pixels == legacy_text[i].pixels && defaults[i].weight == legacy_text[i].weight && defaults[i].style == legacy_text[i].style,
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
