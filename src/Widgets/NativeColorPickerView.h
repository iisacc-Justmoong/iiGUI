#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <array>
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
template<class Policy>class BasicNativeColorPicker final {
    using ColorPickerInput=typename Policy::Input;using ColorPickerConfiguration=typename Policy::Configuration;using ColorPickerLayout=typename Policy::Layout;
    using ColorPickerRect=typename Policy::Rect;using ColorSelectionSnapshot=typename Policy::Snapshot;using StyleColor=typename Policy::Color;
    using ColorPicker=typename Policy::Picker;using NativeColorSurface=typename Policy::Surface;using NativeColorChannel=typename Policy::ChannelView;
    using NativeColorPickerValues=typename Policy::Values;using NativeButton=typename Policy::Button;using ColorSurfaceInput=typename Policy::SurfaceInput;
    using ColorChannelInput=typename Policy::ChannelInput;using ColorPickerValuesInput=typename Policy::ValuesInput;using ButtonInput=typename Policy::ButtonInput;
    using ColorSurfaceKind=typename Policy::SurfaceKind;using ColorChannel=typename Policy::Channel;using ButtonTone=typename Policy::ButtonTone;
    using NativeColorPickerCallbacks=typename Policy::Callbacks;using ColorPickerNativeLayout=typename Policy::LayoutModel;
    struct State final {
        ColorPickerInput authored;
        ColorPickerLayout layout;
        ColorSelectionSnapshot snapshot;
        ii::gui::Widget root, divider, recent_label, color_space;
        std::array<ii::gui::Widget, 10> recent;
        std::optional<NativeColorSurface> surface, rail;
        std::array<std::optional<NativeColorChannel>, 4> channels;
        std::optional<NativeColorPickerValues> values;
        std::optional<NativeButton> eyedropper, cancel, apply;
        NativeColorPickerCallbacks callbacks;
        std::optional<ii::core::Error> failure;
        std::optional<ColorPickerConfiguration> pending_configuration;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, action_epoch = 0;
        bool live = false;
    };
    explicit BasicNativeColorPicker(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::gui::Rect rect(ColorPickerRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::gui::WidgetColor host_color(StyleColor value) noexcept {
        return {value.red, value.green, value.blue, value.alpha};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor value) noexcept {
        const auto convert = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(convert(value.red)), float(convert(value.green)), float(convert(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static ii::core::Result<ColorSelectionSnapshot> read(const ColorPickerInput& input) {
        if (!input.session || input.source_revision < 0 || !ColorPicker::validate(input.configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI color picker configuration");
        auto snapshot = input.session->picker()->editor()->snapshot();
        if (!Policy::valid_color(snapshot.color) || !Policy::valid_color(snapshot.baseline))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Color editor returned an invalid snapshot");
        if (input.session->revision() != snapshot.revision)
            return ii::core::fail(ii::core::ErrorCode::busy, "Color picker changed during measurement");
        return snapshot;
    }
    [[nodiscard]] static ColorSurfaceInput surface_options(const State& state, bool rail) {
        const auto& layout = state.layout; ColorSurfaceInput result;
        result.source = state.authored.session; result.source_revision = state.authored.source_revision;
        result.configuration.kind = rail ? layout.rail_kind.value_or(ColorSurfaceKind::hue_rail) : layout.surface_kind;
        const auto frame = rail && layout.rail_kind ? layout.rail : layout.surface;
        result.configuration.width = rail && !layout.rail_kind ? 24 : frame.width;
        result.configuration.height = frame.height; result.configuration.enabled = state.authored.configuration.enabled;
        result.configuration.accessibility_name = rail ? "Color adjustment" : "Choose color";
        return result;
    }
    [[nodiscard]] static ColorChannelInput channel_options(const State& state, std::size_t index) {
        ColorChannelInput result; result.source = state.authored.session;
        result.source_revision = state.authored.source_revision;
        result.channel = index < state.layout.channels.size() ? state.layout.channels[index].first : ColorChannel::red;
        result.width = state.authored.configuration.width - state.authored.configuration.padding * 2;
        result.enabled = state.authored.configuration.enabled;
        return result;
    }
    [[nodiscard]] static ColorPickerValuesInput value_options(const State& state) {
        return {state.authored.session, state.authored.source_revision,
                state.authored.configuration.width - state.authored.configuration.padding * 2,
                state.authored.configuration.enabled};
    }
    [[nodiscard]] static ButtonInput button_options(const State& state, unsigned kind) {
        ButtonInput result; auto& c = result.configuration;
        c.enabled = state.authored.configuration.enabled; c.height = 22; c.width = kind == 0 ? 22 : 68;
        c.tone = kind == 2 ? ButtonTone::primary : ButtonTone::normal;
        if (kind == 0) {
            result.icon_name = "colorEyedropper"; c.icon_mode = true; c.icon_size = 14; c.horizontal_padding = 4;
            c.accessibility_name = "Pick a color from the document";
        } else {
            c.text = kind == 1 ? state.authored.configuration.cancel_text : state.authored.configuration.apply_text;
            c.accessibility_name = c.text;
        }
        return result;
    }
    [[nodiscard]] static ii::gui::ActionOptions recent_options(const State& state, std::size_t index) {
        const auto color = index < state.authored.configuration.recent_colors.size()
            ? state.authored.configuration.recent_colors[index] : StyleColor{0, 0, 0, 1};
        const auto width = index < state.layout.recent.size() ? state.layout.recent[index].width : 1;
        return {.accessibility_name = "Recent color " + ColorPicker::format_hex(color),
                .enabled = state.authored.configuration.enabled, .focus_ring = false, .width = width, .height = 20};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> recent_paint(
            const std::shared_ptr<State>& state, std::size_t index, const ii::gui::WidgetPaintState& input) {
        try {
            auto result = std::make_shared<ii::gui::WidgetDrawing>();
            if (index >= state->authored.configuration.recent_colors.size())
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
            const auto width = float(input.size.width), height = float(input.size.height);
            for (int y = 0; y < int(std::ceil(height / 4)); ++y) for (int x = 0; x < int(std::ceil(width / 4)); ++x) {
                const auto checker = (x + y) % 2 ? ii::gui::Color{.28f, .28f, .28f, 1} : ii::gui::Color{.56f, .56f, .56f, 1};
                result->panels.push_back({{float(x * 4), float(y * 4), (std::min)(4.f, width - x * 4),
                                           (std::min)(4.f, height - y * 4)}, checker, checker, 0});
            }
            const auto value = state->authored.configuration.recent_colors[index]; auto fill = linear(value);
            if (!input.enabled) fill.alpha *= .5f;
            result->panels.push_back({{0, 0, width, height}, fill, fill, 4});
            const auto selected = value == state->snapshot.color;
            const auto border = input.focused ? ii::gui::Color{.006f, .22f, 1, 1} :
                                selected ? ii::gui::Color{1, 1, 1, .8f} : ii::gui::Color{1, 1, 1, .12f};
            result->panels.push_back({{0, 0, width, height}, border, border, 4, input.focused || selected ? 1.5f : .5f});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Recent color paint failed", std::current_exception());
        }
    }
    [[nodiscard]] static ii::core::Result<bool> apply_host(const std::shared_ptr<State>& state, std::uint64_t revision) {
        const auto step = [&](auto&& action) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = action(); if (!result) return std::unexpected(result.error()); return state->revision == revision;
        };
        const auto& l = state->layout;
        auto result = step([&] { return state->root.set_extent({l.width, l.height}); }); if (!result || !*result) return result;
        result = step([&] { return state->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{
            .name = state->authored.configuration.accessibility_name}); }); if (!result || !*result) return result;
        result = step([&] { return state->surface->set_options(surface_options(*state, false)); }); if (!result || !*result) return result;
        result = step([&] { return state->surface->root().set_frame(rect(l.surface)); }); if (!result || !*result) return result;
        result = step([&] { return state->rail->set_options(surface_options(*state, true)); }); if (!result || !*result) return result;
        result = step([&] { return state->rail->root().set_visible(bool(l.rail_kind)); }); if (!result || !*result) return result;
        if (l.rail_kind) { result = step([&] { return state->rail->root().set_frame(rect(l.rail)); }); if (!result || !*result) return result; }
        for (std::size_t i = 0; i < state->channels.size(); ++i) {
            result = step([&] { return state->channels[i]->set_options(channel_options(*state, i)); }); if (!result || !*result) return result;
            result = step([&] { return state->channels[i]->root().set_visible(i < l.channels.size()); }); if (!result || !*result) return result;
            if (i < l.channels.size()) { result = step([&] { return state->channels[i]->root().set_frame(rect(l.channels[i].second)); }); if (!result || !*result) return result; }
        }
        result = step([&] { return state->divider.set_frame(rect(l.divider)); }); if (!result || !*result) return result;
        result = step([&] { return state->values->set_options(value_options(*state)); }); if (!result || !*result) return result;
        result = step([&] { return state->values->root().set_frame({float(l.comparison.x), float(l.comparison.y),
            float(state->authored.configuration.width - state->authored.configuration.padding * 2), 28}); }); if (!result || !*result) return result;
        result = step([&] { return state->recent_label.set_visible(state->authored.configuration.show_recent_colors); }); if (!result || !*result) return result;
        if (state->authored.configuration.show_recent_colors) { result = step([&] { return state->recent_label.set_frame(rect(l.recent_label)); }); if (!result || !*result) return result; }
        for (std::size_t i = 0; i < state->recent.size(); ++i) {
            const bool shown = state->authored.configuration.show_recent_colors && i < l.recent.size();
            result = step([&] { return state->recent[i].set_action_options(recent_options(*state, i)); }); if (!result || !*result) return result;
            result = step([&] { return state->recent[i].set_visible(shown); }); if (!result || !*result) return result;
            if (shown) { result = step([&] { return state->recent[i].set_frame(rect(l.recent[i])); }); if (!result || !*result) return result; }
            result = step([&] { return state->recent[i].request_repaint(); }); if (!result || !*result) return result;
        }
        for (unsigned i = 0; i < 3; ++i) {
            auto& button = i == 0 ? *state->eyedropper : i == 1 ? *state->cancel : *state->apply;
            result = step([&] { return button.set_options(button_options(*state, i)); }); if (!result || !*result) return result;
            result = step([&] { return button.root().set_visible(state->authored.configuration.show_actions); }); if (!result || !*result) return result;
            if (state->authored.configuration.show_actions) {
                const auto frame = i == 0 ? l.eyedropper : i == 1 ? l.cancel : l.apply;
                result = step([&] { return button.root().set_frame(rect(frame)); }); if (!result || !*result) return result;
            }
        }
        result = step([&] { return state->color_space.set_visible(state->authored.configuration.show_actions); }); if (!result || !*result) return result;
        if (state->authored.configuration.show_actions) result = step([&] { return state->color_space.set_frame(rect(l.color_space)); });
        return result;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ColorPickerInput input) {
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Color picker revision exhausted");
            if (input.preserve_recent_colors && input.session == state->authored.session)
                input.configuration.recent_colors = state->authored.configuration.recent_colors;
            const auto observed = state->revision; auto next = read(input); if (!next) return std::unexpected(next.error());
            auto layout = ColorPickerNativeLayout::compute(input.configuration); if (!layout) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color picker layout");
            if (state->revision != observed) return {};
            if (state->live && state->authored.session != input.session && state->authored.session->picker()->active())
                return ii::core::fail(ii::core::ErrorCode::busy, "Complete the color edit before replacing the picker session");
            auto next_picker = input.session->picker(); const auto old_config = next_picker->configuration();
            auto pending = input.configuration;
            if (state->live && !next_picker->set_configuration(input.configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color picker configuration");
            auto old_input = state->authored; auto old_layout = state->layout; auto old_snapshot = state->snapshot;
            const auto revision = ++state->revision; state->authored = std::move(input); state->layout = std::move(*layout); state->snapshot = *next;
            auto applied = apply_host(state, revision);
            if (!applied) {
                if (state->revision == revision) {
                    if (state->live) (void)next_picker->set_configuration(old_config);
                    state->authored = std::move(old_input);
                    state->layout = std::move(old_layout); state->snapshot = old_snapshot; (void)apply_host(state, revision);
                }
                return std::unexpected(applied.error());
            }
            if (!state->live) state->pending_configuration = std::move(pending);
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Color picker update failed", std::current_exception()); }
    }
    static void edited(const std::shared_ptr<State>& state, StyleColor value) noexcept {
        try {
            const auto revision = state->revision, source = state->authored.session->revision();
            auto result = update(state, state->authored);
            if (!result) { state->failure = result.error(); return; }
            if (state->live && state->revision == revision + 1 && state->snapshot.revision == source &&
                state->snapshot.color == value && state->callbacks.edited) {
                auto callback = state->callbacks.edited; callback(host_color(value));
            }
        } catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Color picker edit callback failed", std::current_exception()}; } catch (...) {} }
    }
    [[nodiscard]] static ii::core::Result<void> action(const std::shared_ptr<State>& state, unsigned kind, std::size_t index = 0) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker requires its owner thread");
        if (!state->live || !state->authored.configuration.enabled) return ii::core::fail(ii::core::ErrorCode::closed, "Color picker is not active");
        if (state->action_epoch == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Color picker action epoch exhausted");
        const auto epoch = ++state->action_epoch; auto session = state->authored.session;
        if (auto focus = state->root.focus(); !focus) return focus;
        if (!state->live || state->action_epoch != epoch || state->authored.session != session) return {};
        if (kind == 3) {
            if (index >= state->authored.configuration.recent_colors.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Recent color index is out of range");
            const auto current = session->picker()->editor()->snapshot();
            auto prepared = session->prepare(current.revision);
            if (!prepared) return ii::core::fail(ii::core::ErrorCode::busy, prepared.error().message);
            auto changed = session->picker()->select_recent_if_current(*prepared, index);
            if (!changed) return ii::core::fail(ii::core::ErrorCode::busy, changed.error().message);
            auto after = session->picker()->editor()->snapshot();
            if (after.revision == *changed && after.color != current.color) edited(state, after.color);
            else if (auto refreshed = update(state, state->authored); !refreshed) return refreshed;
            return {};
        }
        if (kind == 0) {
            if (state->callbacks.eyedropper_requested) { auto callback = state->callbacks.eyedropper_requested; callback(); }
            return {};
        }
        auto picker = session->picker(); auto before = picker->editor()->snapshot();
        if (kind == 2) {
            if (!picker->active()) { auto prepared = session->prepare(before.revision); if (!prepared) return ii::core::fail(ii::core::ErrorCode::busy, prepared.error().message); }
            auto result = picker->accept(); if (!result) return ii::core::fail(ii::core::ErrorCode::busy, result.error().message);
            state->authored.configuration.recent_colors = picker->configuration().recent_colors;
            if (auto refreshed = update(state, state->authored); !refreshed) return refreshed;
            if (state->live && state->action_epoch == epoch && state->callbacks.accepted) { auto callback = state->callbacks.accepted; callback(host_color(result->color)); }
        } else {
            if (picker->active()) {
                auto result = picker->cancel(); if (!result) return ii::core::fail(ii::core::ErrorCode::busy, result.error().message);
                if (auto refreshed = update(state, state->authored); !refreshed) return refreshed;
                if (state->live && state->action_epoch == epoch && before.color != result->color && state->callbacks.edited) {
                    auto callback = state->callbacks.edited; callback(host_color(result->color));
                }
            }
            if (state->live && state->action_epoch == epoch && state->callbacks.canceled) { auto callback = state->callbacks.canceled; callback(); }
        }
        return {};
    }
    static void action_event(const std::shared_ptr<State>& state, unsigned kind, std::size_t index = 0) noexcept {
        try { auto result = action(state, kind, index); if (!result && state->live) state->failure = result.error(); }
        catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Color picker action failed", std::current_exception()}; } catch (...) {} }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeColorPicker> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, ColorPickerInput input, NativeColorPickerCallbacks callbacks = {}) {
        try {
            auto snapshot = read(input); if (!snapshot) return std::unexpected(snapshot.error());
            auto layout = ColorPickerNativeLayout::compute(input.configuration); if (!layout) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid color picker layout");
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->layout = std::move(*layout);
            state->snapshot = *snapshot; state->callbacks = std::move(callbacks);
            state->pending_configuration = state->authored.configuration; std::weak_ptr<State> weak = state;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto root = builder.absolute_stack(parent, id, {state->layout.width, state->layout.height});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto escape = root->set_escape_action([weak] { if (auto state = weak.lock(); state && state->live) action_event(state, 1); }); !escape) return std::unexpected(escape.error());
            auto on_edit = [weak](StyleColor value) { if (auto state = weak.lock(); state && state->live) edited(state, value); };
            auto on_surface = [on_edit](ii::gui::WidgetColor value) { on_edit({value.red, value.green, value.blue, value.alpha}); };
            auto surface = NativeColorSurface::create(builder, *root, name("surface"), surface_options(*state, false), on_surface);
            if (!surface) return std::unexpected(surface.error()); state->surface = *surface;
            auto rail = NativeColorSurface::create(builder, *root, name("rail"), surface_options(*state, true), on_surface);
            if (!rail) return std::unexpected(rail.error()); state->rail = *rail;
            for (std::size_t i = 0; i < state->channels.size(); ++i) {
                auto channel = NativeColorChannel::create(builder, *root, name("channel-" + std::to_string(i)), channel_options(*state, i), on_edit);
                if (!channel) return std::unexpected(channel.error()); state->channels[i] = *channel;
            }
            auto values = NativeColorPickerValues::create(builder, *root, name("values"), value_options(*state), on_edit);
            if (!values) return std::unexpected(values.error()); state->values = *values;
            auto divider = builder.absolute_stack(*root, name("divider"), {1, 1}); if (!divider) return std::unexpected(divider.error()); state->divider = *divider;
            if (auto paint = divider->set_paint_handler([](const ii::gui::WidgetPaintState& input) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto result = std::make_shared<ii::gui::WidgetDrawing>(); const ii::gui::Color color{1, 1, 1, .11f};
                    result->panels.push_back({{0, 0, float(input.size.width), float(input.size.height)}, color, color, 0});
                    return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
                }); !paint) return std::unexpected(paint.error());
            auto label = [&](std::string_view suffix, std::string text) {
                return builder.label(*root, name(suffix), {.text = std::move(text), .font_size = 11, .selectable = false,
                    .role = ii::gui::TextRole::caption, .line_height = 11, .font_weight = 400});
            };
            auto recent_label = label("recent-label", "Recent"); if (!recent_label) return std::unexpected(recent_label.error()); state->recent_label = *recent_label;
            auto color_space = label("color-space", "sRGB"); if (!color_space) return std::unexpected(color_space.error()); state->color_space = *color_space;
            for (std::size_t i = 0; i < state->recent.size(); ++i) {
                auto item = builder.action_surface(*root, name("recent-" + std::to_string(i)), recent_options(*state, i),
                    [weak, i] { if (auto state = weak.lock(); state && state->live) action_event(state, 3, i); });
                if (!item) return std::unexpected(item.error()); state->recent[i] = *item;
                if (auto paint = item->set_paint_handler([weak, i](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                        auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Color picker retired");
                        return recent_paint(state, i, view);
                    }); !paint) return std::unexpected(paint.error());
            }
            for (unsigned i = 0; i < 3; ++i) {
                auto button = NativeButton::create(builder, *root, name(i == 0 ? "eyedropper" : i == 1 ? "cancel" : "apply"), button_options(*state, i),
                    [weak, i] { if (auto state = weak.lock(); state && state->live) action_event(state, i); });
                if (!button) return std::unexpected(button.error());
                (i == 0 ? state->eyedropper : i == 1 ? state->cancel : state->apply) = *button;
            }
            auto applied = apply_host(state, state->revision); if (!applied) return std::unexpected(applied.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock();state&&state->authored.session) {
                    try {
                        if (state->pending_configuration && state->authored.session->picker()->set_configuration(std::move(*state->pending_configuration))) {
                            state->pending_configuration.reset(); state->live = true;
                        } else state->failure = ii::core::Error{ii::core::ErrorCode::invalid_argument, "Invalid staged color picker configuration"};
                    } catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Color picker publication failed", std::current_exception()}; } catch (...) {} }
                }}); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) {
                    const bool was_live = state->live; state->live = false;auto session=state->authored.session;
                    if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                    if(state->action_epoch!=(std::numeric_limits<std::uint64_t>::max)())++state->action_epoch;
                    try { if (was_live && session && session->picker()->active()) (void)session->picker()->cancel(); } catch (...) {}
                    state->authored.session.reset();state->callbacks={};state->pending_configuration.reset();
                }}); !retired) return std::unexpected(retired.error());
            return BasicNativeColorPicker(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Color picker construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget surface() const { return state_->surface->root(); }
    [[nodiscard]] ii::gui::Widget rail() const { return state_->rail->root(); }
    [[nodiscard]] ii::gui::Widget channel_field(std::size_t index) const { return state_->channels.at(index)->field(); }
    [[nodiscard]] ii::gui::Widget hex_field() const { return state_->values->hex_field(); }
    [[nodiscard]] ii::gui::Widget alpha_field() const { return state_->values->alpha_field(); }
    [[nodiscard]] ii::gui::Widget recent(std::size_t index) const { return state_->recent.at(index); }
    [[nodiscard]] ii::gui::Widget apply_button() const { return state_->apply->root(); }
    [[nodiscard]] ii::gui::Widget cancel_button() const { return state_->cancel->root(); }
    [[nodiscard]] ii::gui::Widget eyedropper_button() const { return state_->eyedropper->root(); }
    [[nodiscard]] ii::core::Result<ColorPickerInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Color picker options failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> set_options(ColorPickerInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker requires its owner thread");
        return update(state_, std::move(input));
    }
    [[nodiscard]] ii::core::Result<void> accept() const { return action(state_, 2); }
    [[nodiscard]] ii::core::Result<void> cancel() const { return action(state_, 1); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color picker requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
