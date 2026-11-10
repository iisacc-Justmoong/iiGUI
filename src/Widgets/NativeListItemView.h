#pragma once
#include <iiGUI/ListItemDrawing.h>
#include <iiGUI/NativeButtonView.h>
#include <algorithm>
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
struct ListItemInput final {
    ListRowConfiguration configuration;
    std::shared_ptr<const RasterImage> icon,chevron;
    std::shared_ptr<const ButtonIconSource> icon_source,chevron_source;
    std::string chevron_name="chevron";
    std::int64_t icon_revision=0;
    bool custom_content=false;
    std::optional<std::string> accessibility_description;
    friend bool operator==(const ListItemInput&,const ListItemInput&)=default;
};
struct NativeListItemCallbacks final {std::function<void()> activated;};
struct DefaultListItemHostPolicy final {
    using Model=ListItemModel;using Input=ListItemInput;using Drawing=ListItemDrawing;
    using IconCache=ButtonIconCache;using Callbacks=NativeListItemCallbacks;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept{return DefaultListItemModelPolicy::valid_text(value);}
    [[nodiscard]] static std::string display_text(std::string_view value,std::size_t limit=8192) {
        auto length=(std::min)(limit,value.size());
        while(length<value.size()&&length&&(static_cast<unsigned char>(value[length])&0xc0)==0x80)--length;
        return std::string(value.substr(0,length));
    }
    [[nodiscard]] static core::Result<std::shared_ptr<const RasterImage>> resolve_icon(
            IconCache& cache,const Input& input,const ListRowConfiguration& c) {
        return cache.resolve(c.icon_name,input.icon_source,c.icon_size);
    }
    [[nodiscard]] static core::Result<std::shared_ptr<const RasterImage>> resolve_chevron(
            IconCache& cache,const Input& input,const ListRowConfiguration& c) {
        if(input.chevron)return input.chevron;
        return cache.resolve(input.chevron_name,input.chevron_source,c.chevron_size);
    }
    [[nodiscard]] static ActionOptions action(const Drawing& drawing){return ii::gui::list_item_action(drawing);}
    [[nodiscard]] static core::Result<std::shared_ptr<const WidgetDrawing>> paint(
            const Drawing& drawing,const WidgetPaintState& state,bool include_content) {
        return ii::gui::list_item_paint(drawing,state,include_content);
    }
};
// The policy supplies authored values and assets. iiGUI owns widget state,
// drawing, input, updates, publication and retirement.
template<class Policy>
class BasicNativeListItem final {
    using Model=typename Policy::Model;
    using ListItemInput=typename Policy::Input;
    using ListItemDrawing=typename Policy::Drawing;
    using NativeIconCache=typename Policy::IconCache;
    using NativeListItemCallbacks=typename Policy::Callbacks;
public:
    using ContentPainter = std::function<ii::core::Result<void>(ii::gui::WidgetDrawing&,const ii::gui::WidgetPaintState&,const ListItemDrawing&)>;
private:
    struct Prepared final { ListItemInput input; ListItemDrawing drawing; };
    struct State final {
        std::shared_ptr<const Prepared> prepared;
        NativeIconCache icons, chevrons;
        ii::gui::Widget root;
        NativeListItemCallbacks callbacks;
        ContentPainter content_painter;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit BasicNativeListItem(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const Prepared>> prepare(const ListItemInput& input, State& state) {
        if (!Model::validate(input.configuration) || input.icon_revision < 0 ||
            (input.accessibility_description&&!Policy::valid_text(*input.accessibility_description)) ||
            (input.icon && !input.configuration.icon_name.empty()) ||
            (!input.configuration.icon_name.empty() && input.configuration.icon_size > 0 && input.configuration.icon_size < 1) ||
            (input.configuration.show_chevron && input.configuration.chevron_size > 0 && input.configuration.chevron_size < 1))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item configuration");
        try {
            auto result = std::make_shared<Prepared>(); result->input = input; auto c = input.configuration;
            c.label = Policy::display_text(c.label); c.description = Policy::display_text(c.description);
            if(c.value)c.value=Policy::display_text(*c.value);
            c.detail_date=Policy::display_text(c.detail_date);c.detail_folder1=Policy::display_text(c.detail_folder1);
            c.detail_folder2=Policy::display_text(c.detail_folder2);c.detail_tag1=Policy::display_text(c.detail_tag1);c.detail_tag2=Policy::display_text(c.detail_tag2);
            c.accessibility_name = Policy::display_text(c.accessibility_name);
            if (c.icon_size > 0) {
                result->drawing.icon = input.icon;
                if (!c.icon_name.empty()) {
                    auto image = Policy::resolve_icon(state.icons,input,c);
                    if (!image) return std::unexpected(image.error()); result->drawing.icon = std::move(*image);
                }
            }
            c.has_icon = bool(result->drawing.icon);
            if (c.show_chevron && c.chevron_size > 0) {
                auto image = Policy::resolve_chevron(state.chevrons,input,c);
                if (!image) return std::unexpected(image.error()); result->drawing.chevron = std::move(*image);
            }
            if (!result->drawing.model.set_configuration(std::move(c)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item display configuration");
            return std::shared_ptr<const Prepared>(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Prepared& prepared){
        auto result=Policy::action(prepared.drawing);
        if(prepared.input.accessibility_description)result.accessibility_description=Policy::display_text(*prepared.input.accessibility_description);
        return result;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ListItemInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->prepared->input == input) return {};
            if (state->revision == (std::numeric_limits<std::uint64_t>::max)()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List item revision exhausted");
            const auto observed = state->revision; auto next = prepare(input, *state); if (!next) return std::unexpected(next.error());
            if (!state->live || state->revision != observed) return {};
            auto previous = state->prepared; const auto revision = ++state->revision; state->prepared = std::move(*next);
            const auto rollback = [&] { if (state->revision == revision) { state->prepared = previous;
                (void)state->root.set_action_options(action(*previous));
                (void)state->root.set_visible(previous->input.configuration.visible); (void)state->root.request_repaint(); } };
            if (auto changed = state->root.set_action_options(action(*state->prepared)); !changed) { rollback(); return changed; }
            if (state->revision != revision) return {};
            if (auto shown = state->root.set_visible(state->prepared->input.configuration.visible); !shown) { rollback(); return shown; }
            if (state->revision != revision) return {};
            if (auto painted = state->root.request_repaint(); !painted) { rollback(); return painted; }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListItem> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, ListItemInput input = {}, NativeListItemCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, *state); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks); std::weak_ptr<State> weak = state;
            auto root = builder.action_surface(parent, id, action(*state->prepared), [weak] {
                if (auto state = weak.lock(); state && state->live) try {
                    const auto& c = state->prepared->input.configuration;
                    if (c.enabled && c.visible && c.interactive && c.selectable && state->callbacks.activated) {
                        auto callback = state->callbacks.activated; callback();
                    }
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "List item activation failed", std::current_exception()}; }
            });
            if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto installed = root->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "List item retired");
                    auto drawing=Policy::paint(state->prepared->drawing,view,!state->prepared->input.custom_content);
                    if(!drawing)return std::unexpected(drawing.error());
                    if(state->prepared->input.custom_content&&state->content_painter){
                        auto writable=std::const_pointer_cast<ii::gui::WidgetDrawing>(*drawing);
                        if(auto result=state->content_painter(*writable,view,state->prepared->drawing);!result)return std::unexpected(result.error());
                    }
                    return drawing;
                }); !installed) return std::unexpected(installed.error());
            if (auto shown = root->set_visible(state->prepared->input.configuration.visible); !shown) return std::unexpected(shown.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return BasicNativeListItem(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ListItemInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<void> set_content_painter(ContentPainter painter)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        auto previous=std::move(state_->content_painter);state_->content_painter=std::move(painter);
        if(auto result=state_->root.request_repaint();!result){state_->content_painter=std::move(previous);return result;}
        return {};
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

using NativeListItem=BasicNativeListItem<DefaultListItemHostPolicy>;
} // namespace ii::gui
