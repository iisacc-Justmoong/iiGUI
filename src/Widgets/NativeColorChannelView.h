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
template<class Policy>class BasicNativeColorChannel final {
    using ColorChannelInput=typename Policy::Input;using ColorChannelFrame=typename Policy::Frame;using ColorChannel=typename Policy::Channel;
    using ColorChannels=typename Policy::Channels;using ColorSurfaceError=typename Policy::SurfaceError;using ColorSurfaceErrorCode=typename Policy::SurfaceErrorCode;
    using ColorChannelSurface=typename Policy::RampSource;using NativeColorSurface=typename Policy::Surface;using ColorSurfaceInput=typename Policy::SurfaceInput;
    using ColorSurfaceKind=typename Policy::SurfaceKind;using NativeInputField=typename Policy::Field;using InputFieldConfiguration=typename Policy::FieldConfiguration;
    using NativeInputFieldCallbacks=typename Policy::FieldCallbacks;using StyleColor=typename Policy::Color;
    struct State final {
        ColorChannelInput authored;
        ColorChannelFrame frame;
        std::shared_ptr<ColorChannelSurface> ramp_source;
        ii::gui::Widget root, label;
        std::optional<NativeColorSurface> ramp;
        std::optional<NativeInputField> field;
        std::function<void(StyleColor)> edited;
        std::optional<ii::core::Error> failure;
        std::string pending;
        std::uint64_t edit_revision = 0, revision = 0;
        const std::thread::id owner = std::this_thread::get_id();
        bool dirty = false, live = false;
    };
    explicit BasicNativeColorChannel(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static std::pair<std::string_view, std::string_view> names(ColorChannel channel) noexcept {
        switch (channel) {
        case ColorChannel::hue: return {"H", "Hue"};
        case ColorChannel::saturation: return {"S", "Saturation"};
        case ColorChannel::brightness: return {"B", "Brightness"};
        case ColorChannel::red: return {"R", "Red"};
        case ColorChannel::green: return {"G", "Green"};
        case ColorChannel::blue: return {"B", "Blue"};
        case ColorChannel::cyan: return {"C", "Cyan"};
        case ColorChannel::magenta: return {"M", "Magenta"};
        case ColorChannel::yellow: return {"Y", "Yellow"};
        case ColorChannel::black: return {"K", "Black"};
        case ColorChannel::alpha: return {"A", "Alpha"};
        case ColorChannel::gray: return {"K", "Gray"};
        }
        return {"", ""};
    }
    [[nodiscard]] static std::string display(double value) { return std::to_string(std::lround(value)); }
    [[nodiscard]] static ii::core::Error source_error(const ColorSurfaceError& value) {
        const auto code = value.code == ColorSurfaceErrorCode::invalid ? ii::core::ErrorCode::invalid_argument :
                          value.code == ColorSurfaceErrorCode::stale ? ii::core::ErrorCode::busy : ii::core::ErrorCode::failed;
        return {code, value.message};
    }
    [[nodiscard]] static ii::core::Result<ColorChannelFrame> read(const ColorChannelInput& input) {
        if (!input.source || !ColorChannels::valid_channel(input.channel) || input.source_revision < 0 ||
            !std::isfinite(input.width) || input.width < 100 || input.width > 1110) {
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI color channel configuration");
        }
        for (unsigned attempt = 0; attempt < 4; ++attempt) {
            const auto before = input.source->revision(); auto frame = input.source->channel_frame(input.channel);
            auto color = input.source->color();
            const auto after = input.source->revision();
            if (before != after || (!frame && frame.error().code == ColorSurfaceErrorCode::stale)) continue;
            if (!frame) return std::unexpected(source_error(frame.error()));
            if (!color) return ii::core::fail(ii::core::ErrorCode::failed, color.error().message);
            if (frame->revision != after) continue;
            if (!ColorChannels::valid_frame(*frame) || frame->channel != input.channel || frame->color != *color) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI color channel frame");
            }
            return *frame;
        }
        return ii::core::fail(ii::core::ErrorCode::busy, "Color channel changed repeatedly during measurement");
    }
    [[nodiscard]] static ColorSurfaceInput ramp_options(const State& state) {
        ColorSurfaceInput value; value.source = state.ramp_source; value.source_revision = state.authored.source_revision;
        value.configuration.kind = ColorSurfaceKind::spectrum; value.configuration.width = state.authored.width - 86;
        value.configuration.height = 22; value.configuration.marker_diameter = 12; value.configuration.enabled = state.authored.enabled;
        value.configuration.accessibility_name = std::string(names(state.authored.channel).second);
        return value;
    }
    [[nodiscard]] static InputFieldConfiguration field_options(const State& state) {
        InputFieldConfiguration value; value.text = state.dirty ? state.pending : display(state.frame.value);
        value.width = 54; value.height = 22; value.enabled = state.authored.enabled;
        value.accessibility_name = std::string(names(state.authored.channel).second) + " value";
        return value;
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state, std::uint64_t revision) {
        const auto step = [&](auto&& operation) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = operation(); if (!result) return std::unexpected(result.error()); return state->revision == revision;
        };
        auto result = step([&] { return state->root.set_extent({state->authored.width, 22}); });
        if (!result || !*result) return result;
        result = step([&] { return state->label.set_label_options({.text = std::string(names(state->authored.channel).first),
            .font_size = 13, .selectable = false, .role = ii::gui::TextRole::body, .line_height = 13, .font_weight = 500}); });
        if (!result || !*result) return result;
        result = step([&] { return state->ramp->set_options(ramp_options(*state)); });
        if (!result || !*result) return result;
        result = step([&] { return state->field->set_options(field_options(*state)); });
        if (!result || !*result || state->dirty) return result;
        return step([&]() -> ii::core::Result<void> {
            const auto expected = display(state->frame.value);
            auto current = state->field->snapshot(); if (!current) return std::unexpected(current.error());
            return current->text == expected ? ii::core::Result<void>{} : state->field->set_text(expected);
        });
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ColorChannelInput input) {
        try {
            if (auto valid = state->root.snapshot(); !valid) return std::unexpected(valid.error());
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Color channel revision exhausted");
            const auto observed = state->revision; auto next = read(input);
            if (!next) return std::unexpected(next.error());
            if (state->revision != observed) return {};
            const bool replace = state->authored.source != input.source || state->authored.channel != input.channel;
            auto ramp_source = replace ? std::make_shared<ColorChannelSurface>(input.source, input.channel) : state->ramp_source;
            auto old_input = state->authored; auto old_frame = state->frame; auto old_source = state->ramp_source;
            auto old_pending = state->pending; const bool old_dirty = state->dirty;
            const auto revision = ++state->revision;
            state->authored = std::move(input); state->frame = std::move(*next); state->ramp_source = std::move(ramp_source);
            if (replace || !state->authored.enabled) { state->dirty = false; state->pending.clear(); }
            auto applied = apply(state, revision);
            if (!applied) {
                if (state->revision == revision) {
                    state->authored = std::move(old_input); state->frame = std::move(old_frame); state->ramp_source = std::move(old_source);
                    state->pending = std::move(old_pending); state->dirty = old_dirty; (void)apply(state, revision);
                }
                return std::unexpected(applied.error());
            }
            state->failure.reset(); return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color channel update failed", std::current_exception());
        }
    }
    [[nodiscard]] static std::optional<double> parse(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r\n"), last = text.find_last_not_of(" \t\r\n");
        if (first == text.npos) return std::nullopt;
        return ii::core::parse_decimal(text.substr(first, last - first + 1));
    }
    [[nodiscard]] static ii::core::Result<void> commit(const std::shared_ptr<State>& state) {
        if (!state->dirty) return {};
        auto value = parse(state->pending); const auto expected = state->edit_revision;
        state->dirty = false; state->pending.clear();
        if (!value) return update(state, state->authored);
        const auto channel = state->authored.channel; auto source = state->authored.source;
        const auto before = state->revision;
        auto changed = source->edit_channel(channel, std::clamp(*value, state->frame.minimum, state->frame.maximum), expected);
        if (state->revision != before) return {};
        if (!changed && changed.error().code != ColorSurfaceErrorCode::stale) return std::unexpected(source_error(changed.error()));
        if (auto refreshed = update(state, state->authored); !refreshed) return refreshed;
        if (changed && *changed != expected && state->revision == before + 1 &&
            state->frame.revision == *changed && state->live && state->edited) {
            auto callback = state->edited; callback(state->frame.color);
        }
        return {};
    }
    static void commit_event(const std::shared_ptr<State>& state) noexcept {
        try { auto result = commit(state); if (!result) state->failure = result.error(); }
        catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Color channel commit failed", std::current_exception()}; } catch (...) {} }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeColorChannel> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            ColorChannelInput input, std::function<void(StyleColor)> edited = {}) {
        try {
            auto frame = read(input); if (!frame) return std::unexpected(frame.error());
            auto state = std::make_shared<State>(); state->authored = std::move(input); state->frame = std::move(*frame);
            state->ramp_source = std::make_shared<ColorChannelSurface>(state->authored.source, state->authored.channel);
            state->edited = std::move(edited); std::weak_ptr<State> weak = state;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto root = builder.hstack(parent, id, {.spacing = 6, .alignment = ii::gui::StackAlignment::center});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            auto holder = builder.vstack(*root, name("label-holder"), {.spacing = 0, .alignment = ii::gui::StackAlignment::center});
            if (!holder) return std::unexpected(holder.error());
            if (auto extent = holder->set_extent({20, 22}); !extent) return std::unexpected(extent.error());
            auto label = builder.label(*holder, name("label"), {.text = std::string(names(state->authored.channel).first), .font_size = 13,
                .selectable = false, .role = ii::gui::TextRole::body, .line_height = 13, .font_weight = 500});
            if (!label) return std::unexpected(label.error()); state->label = *label;
            auto ramp = NativeColorSurface::create(builder, *root, name("ramp"), ramp_options(*state), [weak](ii::gui::WidgetColor color) {
                if (auto state = weak.lock(); state && state->live) {
                    const auto revision = state->revision, source_revision = state->authored.source->revision();
                    auto result = update(state, state->authored);
                    if (!result) { state->failure = result.error(); return; }
                    const StyleColor expected{color.red, color.green, color.blue, color.alpha};
                    if (state->live && state->revision == revision + 1 && state->frame.revision == source_revision &&
                        state->frame.color == expected && state->edited) { auto callback = state->edited; callback(state->frame.color); }
                }
            });
            if (!ramp) return std::unexpected(ramp.error()); state->ramp = *ramp;
            NativeInputFieldCallbacks callbacks;
            callbacks.edited = [weak](std::string text) { if (auto state = weak.lock(); state && state->live) {
                if (!state->dirty) state->edit_revision = state->frame.revision;
                state->dirty = true; state->pending = std::move(text);
            }};
            callbacks.submitted = [weak] { if (auto state = weak.lock(); state && state->live) commit_event(state); };
            callbacks.focus_changed = [weak](bool focused) { if (!focused) {
                if (auto state = weak.lock(); state && state->live) commit_event(state);
            }};
            auto field = NativeInputField::create(builder, *root, name("value"), field_options(*state), std::move(callbacks));
            if (!field) return std::unexpected(field.error()); state->field = *field;
            if (auto extent = root->set_extent({state->authored.width, 22}); !extent) return std::unexpected(extent.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) {state->live = false;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->authored.source.reset();state->ramp_source.reset();state->edited={};state->pending.clear();} }); !retired) return std::unexpected(retired.error());
            return BasicNativeColorChannel(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Color channel construction failed", std::current_exception());
        }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget surface() const { return state_->ramp->root(); }
    [[nodiscard]] ii::gui::Widget field() const { return state_->field->root(); }
    [[nodiscard]] ii::core::Result<void> set_options(ColorChannelInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color channel requires its owner thread");
        return update(state_, std::move(input));
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Color channel requires its owner thread");
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
