#pragma once
#include <iiGUI/NativeInputFieldView.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>class BasicNativeColorPickerValues final {
    using ColorPickerValuesInput=typename Policy::Input;using ColorSelectionSnapshot=typename Policy::Snapshot;
    using StyleColor=typename Policy::Color;using NativeInputField=typename Policy::Field;using InputFieldConfiguration=typename Policy::FieldConfiguration;
    using NativeInputFieldCallbacks=typename Policy::FieldCallbacks;using ColorPicker=typename Policy::Picker;
    using ColorSurfaceErrorCode=typename Policy::SurfaceErrorCode;using ColorSelectionErrorCode=typename Policy::SelectionErrorCode;
    using ColorSelectionError=typename Policy::SelectionError;using ColorChannel=typename Policy::Channel;
    struct Pending final { std::string text; std::uint64_t revision = 0; bool dirty = false; };
    struct State final {
        ColorPickerValuesInput authored;
        ColorSelectionSnapshot snapshot;
        ii::gui::Widget root, comparison, hex_label, alpha_label, suffix;
        std::optional<NativeInputField> hex, alpha;
        std::function<void(StyleColor)> edited;
        std::optional<ii::core::Error> failure;
        Pending hex_pending, alpha_pending;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit BasicNativeColorPickerValues(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Result<ColorSelectionSnapshot> read(const ColorPickerValuesInput& input) {
        if (!input.session || input.source_revision < 0 || !std::isfinite(input.width) || input.width < 228 || input.width > 1024)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color picker values configuration");
        auto snapshot = input.session->picker()->editor()->snapshot();
        if (!Policy::valid_color(snapshot.color) || !Policy::valid_color(snapshot.baseline))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color picker values snapshot");
        if (input.session->revision() != snapshot.revision)
            return ii::core::fail(ii::core::ErrorCode::busy, "Color picker changed while reading its values");
        return snapshot;
    }
    [[nodiscard]] static std::string hex_text(StyleColor color) { return ColorPicker::format_hex(color).substr(1, 6); }
    [[nodiscard]] static std::string alpha_text(StyleColor color) { return std::to_string(std::lround(color.alpha * 100)); }
    [[nodiscard]] static InputFieldConfiguration field_options(const State& state, bool alpha) {
        InputFieldConfiguration result;
        result.text = alpha ? (state.alpha_pending.dirty ? state.alpha_pending.text : alpha_text(state.snapshot.color))
                            : (state.hex_pending.dirty ? state.hex_pending.text : hex_text(state.snapshot.color));
        result.width = alpha ? 56 : (std::max)(40., state.authored.width - 187);
        result.height = 22; result.enabled = state.authored.enabled;
        result.accessibility_name = alpha ? "Opacity percent" : "Hex color, RRGGBB or RRGGBBAA";
        return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor value) noexcept {
        const auto convert = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(convert(value.red)), float(convert(value.green)), float(convert(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> swatch(
            const std::shared_ptr<State>& state, const ii::gui::WidgetPaintState& input) {
        try {
            auto result = std::make_shared<ii::gui::WidgetDrawing>();
            const auto width = float(input.size.width), height = float(input.size.height);
            for (int y = 0; y < int(std::ceil(height / 4)); ++y) for (int x = 0; x < int(std::ceil(width / 4)); ++x) {
                const auto color = (x + y) % 2 ? ii::gui::Color{.28f, .28f, .28f, 1} : ii::gui::Color{.56f, .56f, .56f, 1};
                result->panels.push_back({{float(x * 4), float(y * 4), (std::min)(4.f, width - x * 4),
                                           (std::min)(4.f, height - y * 4)}, color, color, 0});
            }
            const auto left = linear(state->snapshot.baseline), right = linear(state->snapshot.color);
            result->panels.push_back({{0, 0, width / 2, height}, left, left, 0});
            result->panels.push_back({{width / 2, 0, width - width / 2, height}, right, right, 0});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color picker comparison paint failed", std::current_exception());
        }
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state, std::uint64_t revision) {
        const auto step = [&](auto&& action) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = action(); if (!result) return std::unexpected(result.error());
            return state->revision == revision;
        };
        const auto width = state->authored.width;
        auto result = step([&] { return state->root.set_extent({width, 28}); }); if (!result || !*result) return result;
        for (const auto& [widget, bounds] : {
                std::pair{state->comparison, ii::gui::Rect{0, 0, 40, 28}},
                std::pair{state->hex_label, ii::gui::Rect{48, 0, 25, 28}},
                std::pair{state->hex->root(), ii::gui::Rect{81, 3, float((std::max)(40., width - 187)), 22}},
                std::pair{state->alpha_label, ii::gui::Rect{float(width - 98), 0, 10, 28}},
                std::pair{state->alpha->root(), ii::gui::Rect{float(width - 80), 3, 56, 22}},
                std::pair{state->suffix, ii::gui::Rect{float(width - 16), 0, 16, 28}}}) {
            result = step([&] { return widget.set_frame(bounds); }); if (!result || !*result) return result;
        }
        result = step([&] { return state->hex->set_options(field_options(*state, false)); }); if (!result || !*result) return result;
        result = step([&] { return state->alpha->set_options(field_options(*state, true)); }); if (!result || !*result) return result;
        for (const auto alpha : {false, true}) {
            if ((alpha ? state->alpha_pending : state->hex_pending).dirty) continue;
            auto& field = alpha ? *state->alpha : *state->hex;
            const auto expected = alpha ? alpha_text(state->snapshot.color) : hex_text(state->snapshot.color);
            result = step([&]() -> ii::core::Result<void> {
                auto current = field.snapshot(); if (!current) return std::unexpected(current.error());
                return current->text == expected ? ii::core::Result<void>{} : field.set_text(expected);
            });
            if (!result || !*result) return result;
        }
        return step([&] { return state->comparison.request_repaint(); });
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ColorPickerValuesInput input) {
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Color picker values revision exhausted");
            const auto observed = state->revision; auto next = read(input); if (!next) return std::unexpected(next.error());
            if (state->revision != observed) return {};
            auto old_input = state->authored; auto old_snapshot = state->snapshot;
            auto old_hex = state->hex_pending, old_alpha = state->alpha_pending;
            const bool clear = input.session != state->authored.session || !input.enabled;
            const auto revision = ++state->revision; state->authored = std::move(input); state->snapshot = *next;
            if (clear) { state->hex_pending = {}; state->alpha_pending = {}; }
            auto applied = apply(state, revision);
            if (!applied) {
                if (state->revision == revision) {
                    state->authored = std::move(old_input); state->snapshot = old_snapshot;
                    state->hex_pending = std::move(old_hex); state->alpha_pending = std::move(old_alpha);
                    (void)apply(state, revision);
                }
                return std::unexpected(applied.error());
            }
            state->failure.reset(); return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color picker values update failed", std::current_exception());
        }
    }
    [[nodiscard]] static std::optional<double> number(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
        if (first == text.npos) return std::nullopt;
        return ii::core::parse_decimal(text.substr(first, last - first + 1));
    }
    [[nodiscard]] static ii::core::Result<void> commit(const std::shared_ptr<State>& state, bool alpha) {
        auto& pending = alpha ? state->alpha_pending : state->hex_pending;
        if (!pending.dirty) return {};
        const auto text = pending.text; const auto expected = pending.revision; pending = {};
        const auto numeric = alpha ? number(text) : std::optional<double>{};
        if ((alpha && !numeric) || (!alpha && !ColorPicker::parse_hex(text, state->snapshot.color.alpha)))
            return update(state, state->authored);
        const auto before = state->revision; const auto before_color = state->snapshot.color;
        auto session = state->authored.session;
        auto prepared = session->prepare(expected);
        if (state->revision != before) return {};
        if (!prepared) {
            if (prepared.error().code == ColorSurfaceErrorCode::stale) return update(state, state->authored);
            return ii::core::fail(ii::core::ErrorCode::failed, prepared.error().message);
        }
        typename Policy::SelectionResult changed = alpha
            ? [&]() -> typename Policy::SelectionResult {
                auto result = session->edit_channel(ColorChannel::alpha, std::clamp(*numeric, 0., 100.), *prepared);
                if (!result) return std::unexpected(ColorSelectionError{
                    result.error().code == ColorSurfaceErrorCode::stale ? ColorSelectionErrorCode::stale : ColorSelectionErrorCode::invalid,
                    result.error().message});
                return *result;
            }()
            : session->picker()->set_hex_if_current(*prepared, text);
        if (state->revision != before) return {};
        if (!changed && changed.error().code != ColorSelectionErrorCode::stale)
            return ii::core::fail(ii::core::ErrorCode::failed, changed.error().message);
        if (auto refreshed = update(state, state->authored); !refreshed) return refreshed;
        if (changed && state->revision == before + 1 && state->snapshot.revision == *changed &&
            state->snapshot.color != before_color && state->live && state->edited) {
            auto callback = state->edited; callback(state->snapshot.color);
        }
        return {};
    }
    static void commit_event(const std::shared_ptr<State>& state, bool alpha) noexcept {
        try { auto result = commit(state, alpha); if (!result) state->failure = result.error(); }
        catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Color picker value commit failed", std::current_exception()}; } catch (...) {} }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeColorPickerValues> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            ColorPickerValuesInput input, std::function<void(StyleColor)> edited = {}) {
        try {
            auto snapshot = read(input); if (!snapshot) return std::unexpected(snapshot.error());
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->snapshot = *snapshot;
            state->edited = std::move(edited); std::weak_ptr<State> weak = state;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto root = builder.absolute_stack(parent, id, {state->authored.width, 28});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            auto comparison = builder.absolute_stack(*root, name("comparison"), {40, 28});
            if (!comparison) return std::unexpected(comparison.error()); state->comparison = *comparison;
            if (auto painted = comparison->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Color picker values retired");
                    return swatch(state, view);
                }); !painted) return std::unexpected(painted.error());
            auto make_label = [&](std::string_view suffix, std::string text) {
                return builder.label(*root, name(suffix), {.text = std::move(text), .font_size = 11, .selectable = false,
                    .role = ii::gui::TextRole::caption, .line_height = 11, .font_weight = 400});
            };
            auto hex_label = make_label("hex-label", "Hex"); if (!hex_label) return std::unexpected(hex_label.error()); state->hex_label = *hex_label;
            auto alpha_label = make_label("alpha-label", "A"); if (!alpha_label) return std::unexpected(alpha_label.error()); state->alpha_label = *alpha_label;
            auto suffix = make_label("alpha-suffix", "%"); if (!suffix) return std::unexpected(suffix.error()); state->suffix = *suffix;
            const auto callbacks = [weak](bool alpha) {
                NativeInputFieldCallbacks result;
                result.edited = [weak, alpha](std::string text) { if (auto state = weak.lock(); state && state->live) {
                    auto& pending = alpha ? state->alpha_pending : state->hex_pending;
                    if (!pending.dirty) pending.revision = state->snapshot.revision;
                    pending.dirty = true; pending.text = std::move(text);
                }};
                result.submitted = [weak, alpha] { if (auto state = weak.lock(); state && state->live) commit_event(state, alpha); };
                result.focus_changed = [weak, alpha](bool focused) { if (!focused) {
                    if (auto state = weak.lock(); state && state->live) commit_event(state, alpha);
                }};
                return result;
            };
            auto hex = NativeInputField::create(builder, *root, name("hex"), field_options(*state, false), callbacks(false));
            if (!hex) return std::unexpected(hex.error()); state->hex = *hex;
            auto alpha = NativeInputField::create(builder, *root, name("alpha"), field_options(*state, true), callbacks(true));
            if (!alpha) return std::unexpected(alpha.error()); state->alpha = *alpha;
            auto applied = apply(state, state->revision); if (!applied) return std::unexpected(applied.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) {state->live = false;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->authored.session.reset();state->edited={};state->hex_pending={};state->alpha_pending={};} }); !retired) return std::unexpected(retired.error());
            return BasicNativeColorPickerValues(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color picker values construction failed", std::current_exception());
        }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget hex_field() const { return state_->hex->root(); }
    [[nodiscard]] ii::gui::Widget alpha_field() const { return state_->alpha->root(); }
    [[nodiscard]] ii::core::Result<void> set_options(ColorPickerValuesInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker values require their owner thread");
        return update(state_, std::move(input));
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker values require their owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
