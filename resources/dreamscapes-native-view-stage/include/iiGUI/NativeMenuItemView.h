#pragma once
#include <iiGUI/MenuItemModel.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
// Applications supply values and icon resolution; the framework owns the host.
template<class Policy>
class BasicNativeMenuItem final {
    using MenuItemModel=typename Policy::Model;
    using MenuItemInput=typename Policy::Input;
    using NativeMenuItemCallbacks=typename Policy::Callbacks;
    using MenuItemPresentation=typename Policy::Presentation;
    using MenuItemActivation=typename Policy::Activation;
    using MenuCloseAction=typename Policy::CloseAction;
    using MenuClosePlan=typename Policy::ClosePlan;
    using MenuPopupPort=typename Policy::PopupPort;
    using MenuItemPort=typename Policy::ItemPort;
    using MenuEntry=typename Policy::Entry;
    using MenuEntryKind=typename Policy::EntryKind;
    using MenuEntryState=typename Policy::EntryState;
    using MenuDirection=typename Policy::Direction;
    using MenuRect=typename Policy::Rect;
    using StyleColor=typename Policy::Color;
    using IconCache=typename Policy::IconCache;
    struct Prepared final {MenuItemModel model;std::shared_ptr<const ii::gui::RasterImage> icon,chevron;double shortcut_width=0;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        MenuItemInput authored;
        std::shared_ptr<IconCache> icons=std::make_shared<IconCache>(),chevrons=std::make_shared<IconCache>();
        Snapshot prepared;
        MenuItemPresentation presentation;
        ii::gui::Widget root;
        NativeMenuItemCallbacks callbacks;
        MenuItemActivation group_activation;
        std::function<ii::core::Result<void>()> group_changed;
        MenuClosePlan group_close_plan;
        std::optional<MenuPopupPort> submenu;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeMenuItem(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const MenuItemInput& input,MenuItemPresentation presentation,State& state){
        if(!MenuItemModel::validate(input.configuration)||input.icon_revision<0||(input.icon&&!input.configuration.icon_name.empty())||
           (input.icon_source&&input.configuration.icon_name.empty())||
           (state.submenu&&!input.configuration.has_children)||
           (presentation.width&&(!std::isfinite(*presentation.width)||*presentation.width<1||*presentation.width>8192)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Menu item configuration");
        auto c=input.configuration;
        if(presentation.selected&&c.state!=MenuEntryState::inactive)c.state=*presentation.selected?MenuEntryState::selected:MenuEntryState::normal;
        if(presentation.compact)c.compact=*presentation.compact;
        if(presentation.show_icon_slot)c.show_icon_slot=*presentation.show_icon_slot;
        if(presentation.width)c.width=*presentation.width;
        auto next=std::make_shared<Prepared>();(void)next->model.set_configuration(c);
        if(c.show_icon_slot){
            next->icon=input.icon;
            if(!c.icon_name.empty()){auto cache=state.icons;if(!cache)return ii::core::fail(ii::core::ErrorCode::closed,"Menu item has retired");
                auto icon=cache->resolve(c.icon_name,input.icon_source,c.icon_size);
                if(!state.icons)return ii::core::fail(ii::core::ErrorCode::closed,"Menu item retired during icon resolution");
                if(!icon)return std::unexpected(icon.error());next->icon=std::move(*icon);}
        }
        if(c.show_chevron&&c.has_children){
            const auto name=Policy::chevron_name(next->model.direction());
            if(name.empty())return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid menu item direction");
            auto cache=state.chevrons;if(!cache)return ii::core::fail(ii::core::ErrorCode::closed,"Menu item has retired");
            auto chevron=cache->resolve(name,{},c.chevron_size);
            if(!state.chevrons)return ii::core::fail(ii::core::ErrorCode::closed,"Menu item retired during icon resolution");
            if(!chevron)return std::unexpected(chevron.error());next->chevron=std::move(*chevron);
        }
        if(!next->model.shortcut().empty()){
            auto measured=ii::gui::measure_text_layout({.text=std::string(next->model.shortcut()),.font_size=float(c.font_size),
                .font_weight=c.font_weight,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());next->shortcut_width=(std::min)(8192.,measured->width);
        }
        const auto left=c.horizontal_padding+(c.show_icon_slot?c.icon_size+c.spacing:0);
        const auto right=c.horizontal_padding+(next->chevron?c.chevron_size+c.spacing:0)+(next->shortcut_width?next->shortcut_width+c.spacing:0);
        if(left>8192||right>8192)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"MenuItem content padding exceeds host limits");
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::core::Result<bool> open_submenu(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
        if(!state->submenu)return false;
        auto anchor=state->root.snapshot();if(!anchor)return std::unexpected(anchor.error());
        if(!anchor->visible||!anchor->enabled||anchor->input_blocked)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Submenu owner is unavailable");
        MenuCloseAction parent;
        if(state->group_close_plan){auto plan=state->group_close_plan();if(!plan)return std::unexpected(plan.error());parent=std::move(*plan);}
        auto opened=state->submenu->open(state->root,state->prepared->model.direction(),std::move(parent));
        if(!opened)return std::unexpected(opened.error());state->failure.reset();return true;
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->model.configuration();const auto chevron=value->chevron?c.chevron_size+c.spacing:0;
        return {.text=c.label,.accessibility_name=c.accessibility_name.empty()?(c.label.empty()?"Menu item":c.label):c.accessibility_name,
            .enabled=c.enabled,.focus_ring=false,.role=ii::gui::ActionRole::menu_item,.text_role=ii::gui::TextRole::body,
            .font_size=c.font_size,.line_height=c.line_height,
            .padding={c.horizontal_padding+(c.show_icon_slot?c.icon_size+c.spacing:0),0,
                      c.horizontal_padding+chevron+(value->shortcut_width?value->shortcut_width+c.spacing:0),0},
            .minimum_width=value->model.width(),.height=value->model.height(),.interaction_enabled=c.state!=MenuEntryState::inactive,
            .checked=c.state==MenuEntryState::selected,.accessibility_description=std::string(value->model.shortcut())};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(const Snapshot& value,const ii::gui::WidgetPaintState& state){
        try{
            const auto& c=value->model.configuration();const auto visual=value->model.visual({state.enabled,state.hovered,state.pressed,state.focused});
            const auto measure=[&](std::string_view text)->ii::core::Result<ii::gui::TextMetrics>{return ii::gui::measure_text_layout({.text=std::string(text),
                .font_size=float(c.font_size),.font_weight=c.font_weight,.font_family=state.font_family,.line_height=float(c.line_height),.elide_right=true});};
            double lw=0,lh=0,sw=0,sh=0;
            if(!c.label.empty()){auto m=measure(c.label);if(!m)return std::unexpected(m.error());lw=(std::min)(32767.,m->width);lh=(std::min)(32767.,m->height);}
            if(!value->model.shortcut().empty()){auto m=measure(value->model.shortcut());if(!m)return std::unexpected(m.error());sw=(std::min)(32767.,m->width);sh=(std::min)(32767.,m->height);}
            auto layout=value->model.geometry(state.size.width,state.size.height,lw,lh,sw,sh);
            if(!layout)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Menu item paint extent");
            const auto rect=[](MenuRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            auto result=std::make_shared<ii::gui::WidgetDrawing>();
            if(visual.background.alpha>0&&layout->surface.width>0&&layout->surface.height>0){const auto ink=linear(visual.background);result->panels.push_back({rect(layout->surface),ink,ink,float(layout->radius)});}
            if(visual.focused&&layout->surface.width>0&&layout->surface.height>0){const auto ink=linear(visual.focus);
                result->panels.push_back({rect(layout->surface),ink,ink,float(layout->radius),float((std::min)({1.5,layout->surface.width/2,layout->surface.height/2}))});}
            if(layout->icon.width>0&&layout->icon.height>0){
                if(value->icon)result->images.push_back({.bounds=rect(layout->icon),.image=value->icon,.opacity=visual.enabled?1.f:.3f});
                else{auto ink=linear(visual.detail);ink.alpha*=.35f;result->panels.push_back({rect(layout->icon),ink,ink,2});}
            }
            if(value->chevron&&layout->chevron.width>0&&layout->chevron.height>0)
                result->images.push_back({.bounds=rect(layout->chevron),.image=value->chevron,.opacity=visual.enabled?1.f:.3f});
            const ii::gui::Rect clip{0,0,float(state.size.width),float(state.size.height)};
            if(layout->label.width>0&&layout->label.height>0)result->texts.push_back({.bounds=rect(layout->label),.text=c.label,.color=linear(visual.text),
                .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,.font_family=state.font_family,
                .line_height=float(c.line_height),.elide_right=true,.clip=clip});
            if(layout->shortcut.width>0&&layout->shortcut.height>0)result->texts.push_back({.bounds=rect(layout->shortcut),.text=std::string(value->model.shortcut()),
                .color=linear(visual.detail),.font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::right,
                .font_family=state.font_family,.line_height=float(c.line_height),.elide_right=true,.clip=clip});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu item paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeMenuItem> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,MenuItemInput input={},NativeMenuItemCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();auto prepared=prepare(input,{},*state);if(!prepared)return std::unexpected(prepared.error());
            state->authored=std::move(input);state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member){return [weak,member]{if(auto value=weak.lock();value&&value->live){auto callback=value->callbacks.*member;if(callback)callback();}};};
            auto root=builder.action_surface(parent,id,action(state->prepared),[weak]{if(auto value=weak.lock();value&&value->live){
                auto callback=value->callbacks.clicked;auto group=value->group_activation;
                auto invoke=[weak,callback=std::move(callback)]{if(auto current=weak.lock();current&&current->live){
                    auto opened=open_submenu(current);if(!opened){current->failure=opened.error();throw std::runtime_error(opened.error().message);}
                    if(callback)callback();}};
                if(group)group(std::move(invoke));else invoke();}});
            if(!root)return std::unexpected(root.error());
            state->root=*root;if(!state->authored.visible)if(auto r=root->set_visible(false);!r)return std::unexpected(r.error());
            if(auto r=root->set_control_events({.pressed=event(&NativeMenuItemCallbacks::pressed),.released=event(&NativeMenuItemCallbacks::released),
                    .canceled=event(&NativeMenuItemCallbacks::canceled)});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state->prepared,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto value=weak.lock()){
                    value->live=false;auto observer=std::move(value->group_changed);
                    if(observer)try{(void)observer();}catch(...){}
                    value->callbacks={};value->group_activation={};value->group_close_plan={};value->submenu.reset();
                    value->authored.icon.reset();value->authored.icon_source.reset();value->prepared.reset();
                    value->icons.reset();value->chevrons.reset();
                }});!r)return std::unexpected(r.error());
            return BasicNativeMenuItem(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu item creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> status()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        if(state_->failure)return std::unexpected(*state_->failure);return {};
    }
    [[nodiscard]] ii::core::Result<void> set_submenu(MenuPopupPort popup)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        if(!popup.snapshot||!popup.open||state_->submenu||!state_->authored.configuration.has_children)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"MenuItem requires one submenu and hasChildItems enabled");
        try{const auto observed=state_->revision;
            const auto superseded=[&]{return state_->revision!=observed||state_->submenu||!state_->authored.configuration.has_children;};
            auto owner=state_->root.snapshot();if(!owner)return std::unexpected(owner.error());
            if(superseded())
                return ii::core::fail(ii::core::ErrorCode::cancelled,"A newer MenuItem update superseded the submenu attachment");
            auto child=popup.snapshot();if(!child)return std::unexpected(child.error());
            if(child->kind!=ii::gui::WidgetKind::popover||child->parent_id!=owner->id)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Submenu must be a direct popup child of its MenuItem");
            if(superseded())
                return ii::core::fail(ii::core::ErrorCode::cancelled,"A newer MenuItem update superseded the submenu attachment");
            auto changed=state_->group_changed;
            if(superseded())
                return ii::core::fail(ii::core::ErrorCode::cancelled,"A newer MenuItem update superseded the submenu attachment");
            const auto revision=state_->revision;state_->submenu=std::move(popup);
            if(changed){
                const auto rollback=[&]{if(state_->revision==revision){state_->submenu.reset();try{(void)changed();}catch(...){}}};
                try{if(auto result=changed();!result){rollback();return result;}}
                catch(...){auto cause=std::current_exception();rollback();
                    return ii::core::fail(ii::core::ErrorCode::failed,"MenuItem submenu notification failed",cause);}
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"MenuItem submenu attachment failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<MenuItemInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"MenuItem options failed",std::current_exception());}
    }
private:
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,MenuItemInput input,MenuItemPresentation presentation){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        try{
            if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"MenuItem revision exhausted");
            const auto observed=state->revision;auto prepared=prepare(input,presentation,*state);if(!prepared)return std::unexpected(prepared.error());
            if(state->revision!=observed)return {};
            auto previous=state->authored;auto old=state->prepared;auto old_presentation=state->presentation;const auto revision=++state->revision;
            state->authored=std::move(input);state->prepared=std::move(*prepared);state->presentation=presentation;
            if(auto result=state->root.set_action_options(action(state->prepared));!result){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old);state->presentation=old_presentation;}return result;}
            if(state->revision!=revision)return {};
            if(previous.visible!=state->authored.visible)if(auto result=state->root.set_visible(state->authored.visible);!result){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old);state->presentation=old_presentation;
                    (void)state->root.set_action_options(action(state->prepared));}return result;}
            if(state->revision!=revision)return {};return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Menu item update failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_options(MenuItemInput input)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
        try{
            auto previous=state_->authored;
            auto result=apply(state_,input,state_->presentation);if(!result)return result;
            if(state_->authored!=input)return {};
            const auto revision=state_->revision;auto changed=state_->group_changed;
            if(changed)if(auto notified=changed();!notified){
                if(state_->revision==revision){auto restored=apply(state_,std::move(previous),state_->presentation);
                    if(restored&&state_->revision==revision+1)(void)changed();}
                return notified;
            }
            return {};
        }
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"MenuItem group notification failed",std::current_exception());}
    }
    [[nodiscard]] MenuItemPort weak_group_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .present=[weak](MenuItemPresentation value)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");return apply(state,state->authored,value);},
            .activated=[weak](MenuItemActivation hook)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(hook&&state->group_activation)return ii::core::fail(ii::core::ErrorCode::already_exists,"MenuItem already belongs to a menu");state->group_activation=std::move(hook);return {};},
            .changed=[weak](std::function<ii::core::Result<void>()> hook)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(hook&&state->group_changed)return ii::core::fail(ii::core::ErrorCode::already_exists,"MenuItem already has a menu observer");state->group_changed=std::move(hook);return {};},
            .entry=[weak]()->ii::core::Result<MenuEntry>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());const auto& input=state->authored;const auto& c=input.configuration;
                return MenuEntry{.key=snapshot->id,.label=c.label,.shortcut=std::string(state->prepared->model.shortcut()),.icon_name=c.icon_name,
                    .accessibility_name=c.accessibility_name,.kind=(state->submenu||(c.has_children&&c.show_chevron))?MenuEntryKind::submenu:MenuEntryKind::action,.state=c.state,
                    .enabled=c.enabled,.visible=input.visible,.compact=c.compact,.height=c.height>0?std::optional<double>{c.height}:std::nullopt,.close_on_trigger=input.close_on_trigger};},
            .snapshot=[weak]()->ii::core::Result<ii::gui::WidgetSnapshot>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");return state->root.snapshot();},
            .focus=[weak]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");return state->root.focus();},
            .open_submenu=[weak]()->ii::core::Result<bool>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");return open_submenu(state);},
            .parent_close=[weak](MenuClosePlan plan)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(plan&&state->group_close_plan)return ii::core::fail(ii::core::ErrorCode::already_exists,"MenuItem already has a menu owner");state->group_close_plan=std::move(plan);return {};},
            .retired=[weak]{auto state=weak.lock();if(!state)return true;if(state->owner!=std::this_thread::get_id())return false;
                auto snapshot=state->root.snapshot();return !snapshot&&snapshot.error().code==ii::core::ErrorCode::closed;},
            .submenu_open=[weak]()->ii::core::Result<bool>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(!state->submenu)return false;auto popup=state->submenu->snapshot();if(!popup)return std::unexpected(popup.error());return popup->opened;},
            .close_submenu=[weak]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(!state->submenu)return {};auto popup=state->submenu->snapshot();if(!popup)return std::unexpected(popup.error());
                if(!popup->opened)return {};if(!state->submenu->close)return ii::core::fail(ii::core::ErrorCode::unsupported,"Submenu adapter cannot close its popup");
                return state->submenu->close();},
            .widget=[weak]()->ii::core::Result<ii::gui::Widget>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"MenuItem has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"MenuItem requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return state->root;}
        };
    }
};
} // namespace ii::gui
