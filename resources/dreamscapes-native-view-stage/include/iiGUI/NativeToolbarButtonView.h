#pragma once
#include <iiGUI/ToolbarButtonModel.h>
#include <iiGUI/NativeButtonView.h>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(ToolbarButtonId value) { return value; }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(std::string value) { return value; }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(std::string_view value) { return std::string(value); }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(const char* value) { return std::string(value ? value : ""); }
template<std::integral T> requires (!std::same_as<T, bool>)
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(T value) {
    if (!std::in_range<std::int64_t>(value)) throw std::out_of_range("Toolbar button ID is out of range");
    return std::int64_t(value);
}
struct ToolbarButtonInput final {
    ToolbarButtonConfiguration configuration;
    std::string icon_name;
    std::shared_ptr<const RasterImage> icon;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const ToolbarButtonInput&,const ToolbarButtonInput&)=default;
};
struct ToolbarButtonCallbacks final {
    std::function<void()> clicked;
    std::function<void(const ToolbarButtonId&)> triggered;
};
struct DefaultToolbarButtonHostPolicy final {
    using Input=ToolbarButtonInput;using Configuration=ToolbarButtonConfiguration;using Model=ToolbarButtonModel;
    using Rect=ToolbarButtonRect;using Tone=ButtonTone;using Color=WidgetColor;using IconCache=ButtonIconCache;
    using Callbacks=ToolbarButtonCallbacks;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return DefaultButtonPolicy::valid_text(value);}
};
template<class Policy>
class BasicNativeToolbarButton final {
    using ToolbarButtonInput=typename Policy::Input;
    using ToolbarButtonConfiguration=typename Policy::Configuration;
    using ToolbarButton=typename Policy::Model;
    using ToolbarButtonRect=typename Policy::Rect;
    using ButtonTone=typename Policy::Tone;
    using StyleColor=typename Policy::Color;
    using NativeIconCache=typename Policy::IconCache;
    using NativeToolbarButtonCallbacks=typename Policy::Callbacks;
    struct Prepared final { ToolbarButtonInput input; ToolbarButtonConfiguration configuration; std::shared_ptr<const ii::gui::RasterImage> icon; };
    struct State final {
        std::shared_ptr<const Prepared> prepared;
        NativeIconCache icons;
        ii::gui::Widget root;
        NativeToolbarButtonCallbacks callbacks;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit BasicNativeToolbarButton(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::gui::Color color(StyleColor value) noexcept {
        const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(channel(value.red)), float(channel(value.green)), float(channel(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static ii::gui::Rect rect(ToolbarButtonRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const Prepared>> prepare(const ToolbarButtonInput& input, NativeIconCache& cache) {
        if (input.icon_revision < 0 || (input.icon && !input.icon_name.empty()) || input.icon_name.size() > 256 || !Policy::valid_text(input.icon_name))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid toolbar button icon");
        try {
            auto next = std::make_shared<Prepared>(); next->input = input; next->configuration = input.configuration;
            next->configuration.has_icon = bool(input.icon) || !input.icon_name.empty();
            if (!ToolbarButton::validate(next->configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid toolbar button configuration");
            next->icon = input.icon;
            if (!input.icon_name.empty()) {
                auto icon = cache.resolve(input.icon_name, input.icon_source, input.configuration.icon_size);
                if (!icon) return std::unexpected(icon.error()); next->icon = std::move(*icon);
            }
            return std::shared_ptr<const Prepared>(std::move(next));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Toolbar button preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const std::shared_ptr<const Prepared>& value) {
        const auto& c = value->configuration; const auto gap = c.has_icon && !c.text.empty() ? c.spacing : 0.;
        auto name = c.accessibility_name.empty() ? (c.text.empty() ? value->input.icon_name : c.text) : c.accessibility_name;
        return {.text=c.text, .accessibility_name=std::move(name), .enabled=c.enabled, .focus_ring=false,
            .role=ii::gui::ActionRole::button, .text_role=ii::gui::TextRole::body, .font_size=c.font_size, .line_height=c.font_size + 2,
            .padding={c.left_padding + (c.has_icon ? c.icon_size + gap : 0), 0, c.right_padding, 0},
            .width=c.width ? std::optional<double>{c.width} : std::nullopt, .height=c.height,
            .interaction_enabled=c.tone != ButtonTone::disabled};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<const Prepared>& value, const ii::gui::WidgetPaintState& state) {
        const auto& c = value->configuration;
        const auto visual = ToolbarButton::visual(c, {state.enabled, state.hovered, state.pressed, state.focused});
        double text_width = 0;
        if (!c.text.empty()) {
            auto measured = ii::gui::measure_text_layout({.text=c.text, .font_size=float(c.font_size), .font_weight=500,
                .font_family=state.font_family, .line_height=float(c.font_size + 2), .elide_right=true});
            if (!measured) return std::unexpected(measured.error()); text_width = (std::min)(8192., double(measured->width));
        }
        auto geometry = ToolbarButton::geometry(c, state.size.width, state.size.height, text_width);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid toolbar button extent");
        auto drawing = std::make_shared<ii::gui::WidgetDrawing>();
        if (state.size.width > 0 && state.size.height > 0 && visual.background.alpha > 0) {
            const auto fill = color(visual.background);
            drawing->panels.push_back({{0, 0, float(state.size.width), float(state.size.height)}, fill, fill, float(geometry->radius)});
        }
        if (visual.focused && state.size.width >= 2 && state.size.height >= 2) {
            const auto focus = color(visual.focus);
            drawing->panels.push_back({{0, 0, float(state.size.width), float(state.size.height)}, focus, focus, float(geometry->radius), 1.5f});
        }
        if (value->icon && geometry->icon.width > 0) drawing->images.push_back({.bounds=rect(geometry->icon), .image=value->icon, .opacity=visual.enabled ? 1.f : .3f});
        if (geometry->text.width > 0 && geometry->text.height > 0) drawing->texts.push_back({.bounds=rect(geometry->text),
            .text=c.text, .color=color(visual.text), .font_size=float(c.font_size), .font_weight=500,
            .alignment=ii::gui::TextAlignment::left, .font_family=state.font_family, .line_height=float(c.font_size + 2), .elide_right=true,
            .clip=ii::gui::Rect{0, 0, float(state.size.width), float(state.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ToolbarButtonInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Toolbar button requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->prepared->input == input) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Toolbar button revision exhausted");
            const auto observed = state->revision; auto next = prepare(input, state->icons); if (!next) return std::unexpected(next.error());
            if (state->revision != observed || !state->live) return {};
            auto previous = state->prepared; const auto revision = ++state->revision; state->prepared = std::move(*next);
            const auto rollback = [&] { if (state->revision == revision) { state->prepared = previous;
                (void)state->root.set_action_options(action(previous)); (void)state->root.set_visible(previous->configuration.visible); (void)state->root.request_repaint(); } };
            if (auto changed = state->root.set_action_options(action(state->prepared)); !changed) { rollback(); return changed; }
            if (state->revision != revision) return {};
            if (auto visible = state->root.set_visible(state->prepared->configuration.visible); !visible) { rollback(); return visible; }
            if (state->revision != revision) return {};
            if (auto repainted = state->root.request_repaint(); !repainted) { rollback(); return repainted; }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Toolbar button update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeToolbarButton> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, ToolbarButtonInput input = {}, NativeToolbarButtonCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, state->icons); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks); std::weak_ptr<State> weak = state;
            auto root = builder.action_surface(parent, id, action(state->prepared), [weak] { if (auto state = weak.lock(); state && state->live) try {
                const auto revision = state->revision; const auto& c = state->prepared->configuration;
                if (!c.enabled || !c.visible || c.tone == ButtonTone::disabled) return;
                auto id = c.id; if (state->callbacks.clicked) { auto callback = state->callbacks.clicked; callback(); }
                if (state->live && state->revision == revision && state->callbacks.triggered) { auto callback = state->callbacks.triggered; callback(id); }
            } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Toolbar button callback failed", std::current_exception()}; } });
            if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto installed = root->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Toolbar button retired"); return paint(state->prepared, view);
                }); !installed) return std::unexpected(installed.error());
            if (auto shown = root->set_visible(state->prepared->configuration.visible); !shown) return std::unexpected(shown.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return BasicNativeToolbarButton(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Toolbar button construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ToolbarButtonInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Toolbar button requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ToolbarButtonInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Toolbar button requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

using ToolbarButtonView=BasicNativeToolbarButton<DefaultToolbarButtonHostPolicy>;
} // namespace ii::gui
