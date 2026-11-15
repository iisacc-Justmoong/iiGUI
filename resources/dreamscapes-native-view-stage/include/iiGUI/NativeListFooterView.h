#pragma once
#include <iiGUI/ListFooterModel.h>
#include <iiGUI/NativeToolbarButtonView.h>
#include <array>
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
struct FooterInput final {
    FooterOptions configuration;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const FooterInput&,const FooterInput&)=default;
};
struct FooterCallbacks final {
    std::function<void(std::size_t,const FooterSlot&)> clicked;
};
struct DefaultListFooterHostPolicy final {
    using Input=FooterInput;using Model=ListFooterModel;using Rect=FooterRect;
    using ButtonModel=ToolbarButtonModel;using ButtonInput=ToolbarButtonInput;using Button=ToolbarButtonView;
    using IconCache=ButtonIconCache;using Callbacks=FooterCallbacks;
    [[nodiscard]] static std::string_view indicator(const FooterOptions& value) noexcept{return value.menu_indicator_text;}
};
template<class Policy>
class BasicNativeListFooter final {
    using FooterInput=typename Policy::Input;
    using ListFooter=typename Policy::Model;
    using FooterRect=typename Policy::Rect;
    using ToolbarButton=typename Policy::ButtonModel;
    using ToolbarButtonInput=typename Policy::ButtonInput;
    using NativeToolbarButton=typename Policy::Button;
    using NativeIconCache=typename Policy::IconCache;
    using NativeListFooterCallbacks=typename Policy::Callbacks;
    struct Prepared final {
        FooterInput input;
        std::array<std::shared_ptr<const ii::gui::RasterImage>, 3> icons;
        std::array<double, 3> text_widths{};
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        std::array<NativeIconCache, 3> icons;
        ii::gui::Widget root;
        std::array<std::optional<NativeToolbarButton>, 3> buttons;
        ii::gui::Size extent;
        NativeListFooterCallbacks callbacks;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit BasicNativeListFooter(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const FooterInput& input, std::array<NativeIconCache, 3>& caches) {
        if (!ListFooter::validate(input.configuration) || input.icon_revision < 0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list footer configuration");
        try {
            auto next = std::make_shared<Prepared>(); next->input = input;
            for (std::size_t i = 0; i < next->icons.size(); ++i) {
                const auto& slot = input.configuration.slots[i];
                if (!slot.icon_name.empty()) {
                    auto icon = caches[i].resolve(slot.icon_name, input.icon_source, input.configuration.icon_size);
                    if (!icon) return std::unexpected(icon.error()); next->icons[i] = std::move(*icon);
                }
                if (!slot.text.empty()) {
                    auto measured = ii::gui::measure_text_layout({.text=slot.text, .font_size=float(input.configuration.font_size),
                        .font_weight=500, .line_height=float(input.configuration.font_size + 2), .elide_right=true});
                    if (!measured) return std::unexpected(measured.error()); next->text_widths[i] = (std::min)(8192., double(measured->width));
                }
                auto button = child_input(next, i, {0, 0, 1, 1});
                auto configuration = button.configuration; configuration.has_icon = bool(button.icon);
                if (!ToolbarButton::validate(configuration)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list footer slot");
            }
            if (!ListFooter::geometry(input.configuration, next->text_widths))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "List footer content exceeds its geometry limit");
            return Snapshot(std::move(next));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List footer preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ToolbarButtonInput child_input(const Snapshot& value, std::size_t index, FooterRect frame) {
        const auto& c = value->input.configuration; const auto& slot = c.slots[index]; ToolbarButtonInput result;
        result.icon = value->icons[index]; auto& button = result.configuration;
        button.id = std::int64_t(index); button.text = slot.text;
        if (slot.menu_indicator) { const auto indicator=Policy::indicator(c); if(!indicator.empty()) { if(!slot.text.empty()) button.text += " "; button.text += indicator; } }
        button.accessibility_name = slot.accessibility_name.empty() ? (slot.text.empty() ? slot.icon_name : slot.text) : slot.accessibility_name;
        button.enabled = c.enabled && c.interactive && slot.enabled && (bool(result.icon) || !button.text.empty());
        button.visible = slot.visible && frame.width > 0 && frame.height > 0;
        button.width = (std::max)(1., frame.width); button.height = (std::max)(1., frame.height);
        button.icon_size = c.icon_size; button.font_size = c.font_size; button.corner_radius = c.corner_radius; button.palette = c.palette;
        return result;
    }
    [[nodiscard]] static ii::gui::Rect rect(FooterRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::core::Result<bool> frames(const std::shared_ptr<State>& state, std::uint64_t revision) {
        auto config = state->prepared->input.configuration;
        if (state->extent.width <= 0 || state->extent.height <= 0) {
            for (std::size_t i = 0; i < state->buttons.size(); ++i) if (state->buttons[i]) {
                if (auto changed = state->buttons[i]->set_options(child_input(state->prepared, i, {})); !changed) return std::unexpected(changed.error());
                if (auto frame = state->buttons[i]->root().set_frame({0, 0, 0, 0}); !frame) return std::unexpected(frame.error());
                if (state->revision != revision) return false;
            }
            return true;
        }
        config.width = state->extent.width; config.height = state->extent.height;
        auto geometry = ListFooter::geometry(config, state->prepared->text_widths);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list footer extent");
        for (std::size_t i = 0; i < state->buttons.size(); ++i) {
            if (!state->buttons[i]) continue;
            auto input = child_input(state->prepared, i, geometry->slots[i]);
            if (auto changed = state->buttons[i]->set_options(std::move(input)); !changed) return std::unexpected(changed.error());
            if (state->revision != revision) return false;
            if (auto frame = state->buttons[i]->root().set_frame(rect(geometry->slots[i])); !frame) return std::unexpected(frame.error());
            if (state->revision != revision) return false;
        }
        return true;
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state, std::uint64_t revision) {
        if (auto extent = state->root.set_extent(state->extent); !extent) return std::unexpected(extent.error());
        if (state->revision != revision) return false;
        return frames(state, revision);
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, FooterInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List footer requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->prepared->input == input) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List footer revision exhausted");
            const auto observed = state->revision; auto next = prepare(input, state->icons); if (!next) return std::unexpected(next.error());
            if (state->revision != observed || !state->live) return {};
            const auto geometry = *ListFooter::geometry(input.configuration, (*next)->text_widths);
            auto previous = state->prepared; const auto extent = state->extent;
            const auto revision = ++state->revision; state->prepared = std::move(*next); state->extent = {geometry.width, geometry.height};
            auto applied = apply(state, revision);
            if (!applied) {
                if (state->revision == revision) { state->prepared = std::move(previous); state->extent = extent; (void)apply(state, revision); }
                return std::unexpected(applied.error());
            }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List footer update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListFooter> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, FooterInput input = {}, NativeListFooterCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, state->icons); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks);
            const auto geometry = *ListFooter::geometry(input.configuration, state->prepared->text_widths);
            state->extent = {geometry.width, geometry.height}; std::weak_ptr<State> weak = state;
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            for (std::size_t i = 0; i < state->buttons.size(); ++i) {
                auto button = NativeToolbarButton::create(builder, *root, std::string(id) + ".slot" + std::to_string(i),
                    child_input(state->prepared, i, geometry.slots[i]), {.clicked = [weak, i] { if (auto state = weak.lock(); state && state->live) try {
                        const auto revision = state->revision; auto slot = state->prepared->input.configuration.slots[i];
                        if (!state->prepared->input.configuration.enabled || !state->prepared->input.configuration.interactive || !slot.enabled || !slot.visible) return;
                        if (slot.command) slot.command->invoke();
                        if (state->live && state->revision == revision && state->callbacks.clicked) { auto callback = state->callbacks.clicked; callback(i, slot); }
                    } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "List footer callback failed", std::current_exception()}; } }});
                if (!button) return std::unexpected(button.error()); state->buttons[i] = *button;
                if (auto frame = button->root().set_frame(rect(geometry.slots[i])); !frame) return std::unexpected(frame.error());
            }
            if (auto layout = root->set_layout_handler([weak](ii::gui::Rect bounds) { if (auto state = weak.lock()) {
                    if (state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; auto result = frames(state, state->revision); if (!result) state->failure = result.error();
                }}); !layout) return std::unexpected(layout.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) {
                    state->live = true; auto result = frames(state, state->revision); if (!result) state->failure = result.error();
                }}); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return BasicNativeListFooter(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List footer construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> slot(std::size_t index) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List footer requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        if (index >= state_->buttons.size() || !state_->buttons[index]) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List footer slot was not found");
        return state_->buttons[index]->root();
    }
    [[nodiscard]] ii::core::Result<FooterInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List footer requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(FooterInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List footer requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

using ListFooterView=BasicNativeListFooter<DefaultListFooterHostPolicy>;
} // namespace ii::gui
