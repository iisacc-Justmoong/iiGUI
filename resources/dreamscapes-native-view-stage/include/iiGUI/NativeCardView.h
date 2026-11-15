#pragma once
#include <iiGUI/CardModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/SrgbColor.h>
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
struct CardInput final {CardConfiguration configuration;std::shared_ptr<const RasterImage> icon,preview;std::shared_ptr<const ButtonIconSource> icon_source;std::int64_t icon_revision=0;
 friend bool operator==(const CardInput&,const CardInput&)=default;};
struct CardCallbacks final {std::function<void()> clicked;std::function<void(bool)> selected_changed;std::function<void()> menu_requested,action_triggered,double_clicked;};
struct CardViews final {WidgetFactory content,preview,icon;};
struct DefaultCardHostPolicy final {using Model=CardModel;using Input=CardInput;using Callbacks=CardCallbacks;using Views=CardViews;
 using Type=CardType;using Rect=CardRect;using Color=WidgetColor;using IconCache=ButtonIconCache;
 [[nodiscard]] static std::string display_text(std::string_view value){auto n=(std::min)(std::size_t(8192),value.size());
  while(n<value.size()&&n&&(static_cast<unsigned char>(value[n])&0xc0)==0x80)--n;return std::string(value.substr(0,n));}};
template<class Policy>class BasicNativeCard final {
    using Card=typename Policy::Model;using CardInput=typename Policy::Input;using NativeCardCallbacks=typename Policy::Callbacks;
    using NativeCardViews=typename Policy::Views;using CardType=typename Policy::Type;using CardRect=typename Policy::Rect;
    using StyleColor=typename Policy::Color;using IconCache=typename Policy::IconCache;
    struct Prepared final { CardInput authored; Card model; std::shared_ptr<const ii::gui::RasterImage> icon, preview; };
    struct ViewSlot final { ii::gui::Widget container,host; };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        std::shared_ptr<IconCache> icons=std::make_shared<IconCache>();
        ii::gui::Widget root,surface,menu,action;
        std::optional<ViewSlot> content,preview,icon;
        NativeCardCallbacks callbacks;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, request_revision = 0;
        double last_press=0,last_x=0,last_y=0;
        std::uint64_t last_device=0;
        bool pending_double=false;
        bool live = false, retired = false, custom_content = false, custom_preview = false, custom_icon = false;
    };
    explicit BasicNativeCard(std::shared_ptr<State> state):state_(std::move(state)) {}
    [[nodiscard]] static ii::gui::Rect rect(CardRect value) noexcept { return {float(value.x),float(value.y),float(value.width),float(value.height)}; }
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const CardInput& input,State& state) {
        if (!Card::validate(input.configuration) || input.icon_revision < 0 || (input.icon && !input.configuration.icon_name.empty()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card configuration");
        try {
            auto next=std::make_shared<Prepared>(); next->authored=input;auto c=input.configuration;
            const auto bound=[](std::string& value){value=Policy::display_text(value);};
            bound(c.title);bound(c.description);bound(c.details);bound(c.metadata);bound(c.status_text);bound(c.summary);
            bound(c.footnote);bound(c.domain);bound(c.preview_title);bound(c.progress_text);bound(c.menu_accessibility_name);
            if(c.action_text)bound(*c.action_text);if(c.progress_label)bound(*c.progress_label);
            for(auto& row:c.rows){bound(row.label);bound(row.value);}
            if(!next->model.set_configuration(std::move(c)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card display state");
            next->icon=input.icon;next->preview=input.preview;
            if(!state.custom_content&&!state.custom_icon&&!input.configuration.icon_name.empty()){
                auto icons=state.icons;if(!icons)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");
                auto image=icons->resolve(input.configuration.icon_name,input.icon_source,40);
                if(!image)return std::unexpected(image.error());next->icon=std::move(*image);
            }
            return Snapshot(std::move(next));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"Card preparation failed",std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::ActionOptions surface_options(const Snapshot& value) {
        const auto& c=value->model.configuration();const auto size=value->model.intrinsic_size();
        const auto name=!c.title.empty()?c.title:!c.preview_title.empty()?c.preview_title:c.domain;
        return {.accessibility_name=name,.enabled=c.enabled,.focus_ring=false,.width=size.width,.height=size.height,
                .checkable=value->model.can_select(),.checked=value->model.can_select()&&c.selected,.accessibility_description=c.description};
    }
    [[nodiscard]] static ii::gui::ActionOptions menu_options(const Snapshot& value) {
        const auto& c=value->model.configuration();return {.accessibility_name=c.menu_accessibility_name,.enabled=c.enabled,.focus_ring=false};
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const Snapshot& value) {
        const auto name=value->model.action_text();
        return {.accessibility_name=name.empty()?"Action":std::string(name),.enabled=value->model.configuration().enabled,.focus_ring=false};
    }
    [[nodiscard]] static ii::gui::Rect crop(const ii::gui::RasterImage& image,CardRect target) noexcept {
        const auto source_ratio=double(image.width())/double(image.height());const auto target_ratio=target.width/target.height;
        if(source_ratio>target_ratio){const auto width=target_ratio/source_ratio;return {float((1-width)/2),0,float(width),1};}
        const auto height=source_ratio/target_ratio;return {0,float((1-height)/2),1,float(height)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_surface(const Snapshot& value,
            const ii::gui::WidgetPaintState& state,bool custom_content,bool custom_preview,bool custom_icon) {
        try {
            const auto geometry=value->model.geometry(state.size.width,state.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card paint extent");
            const auto& g=*geometry;const auto& c=value->model.configuration();auto visual=value->model.visual({state.hovered,state.pressed,state.focused});
            if(!state.enabled){visual.text=c.palette.disabled_text;visual.secondary=c.palette.disabled_text;visual.focused=false;}
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto radius=float((std::min)({value->model.resolved_corner_radius(),state.size.width/2,state.size.height/2}));
            const auto surface=ii::gui::srgb_to_linear(visual.surface),border=ii::gui::srgb_to_linear(visual.border);
            if(g.surface.width>0&&g.surface.height>0){
                if(visual.surface.alpha>0) drawing->panels.push_back({rect(g.surface),surface,surface,radius});
                if(c.type!=CardType::file_preview)drawing->panels.push_back({rect(g.surface),border,border,radius,visual.selected?2.f:1.f});
                if(visual.focused){const auto focus=ii::gui::srgb_to_linear(visual.focus);drawing->panels.push_back({rect(g.surface),focus,focus,radius,1.5f});}
            }
            if(custom_content)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            if(!custom_preview&&value->preview&&g.preview.width>0&&g.preview.height>0)
                drawing->images.push_back({.bounds=rect(g.preview),.image=value->preview,.opacity=state.enabled?1.f:.35f,
                                           .source=crop(*value->preview,g.preview),.radius=radius});
            if(!custom_icon&&!(custom_preview&&c.type==CardType::file)&&value->icon&&(!value->preview||c.type!=CardType::file)&&g.icon.width>0&&g.icon.height>0)
                drawing->images.push_back({.bounds=rect(g.icon),.image=value->icon,.opacity=state.enabled?1.f:.35f});
            const auto clip=rect(g.surface);
            const auto text=[&](CardRect area,std::string_view label,StyleColor color,float size,unsigned weight,ii::gui::TextAlignment align=ii::gui::TextAlignment::left){
                if(area.width<=0||area.height<=0||label.empty())return;
                drawing->texts.push_back({.bounds=rect(area),.text=std::string(label),.color=ii::gui::srgb_to_linear(color),.font_size=size,.font_weight=weight,
                    .alignment=align,.font_family=state.font_family,.line_height=float(area.height),.elide_right=true,.clip=clip});
            };
            text(g.title,c.type==CardType::file_preview?c.preview_title:c.title,visual.text,14,600);text(g.description,c.description,visual.secondary,12,400);
            text(g.details,c.details,visual.secondary,11,400);text(g.metadata,c.metadata,visual.secondary,11,400);
            text(g.status,c.status_text,state.enabled?c.palette.status:c.palette.disabled_text,11,500);
            text(g.summary,c.summary,visual.secondary,11,400);text(g.footnote,c.footnote,visual.secondary,11,400);text(g.domain,c.domain,visual.secondary,11,400);
            for(const auto& row:g.rows){text(row.label,c.rows[row.index].label,visual.secondary,11,400);text(row.value,c.rows[row.index].value,visual.text,11,500,ii::gui::TextAlignment::right);}
            text(g.progress_label,value->model.progress_label(),visual.secondary,11,400);
            text(g.progress_text,c.progress_text,visual.text,11,500,ii::gui::TextAlignment::right);
            if(g.progress_track.width>0&&g.progress_track.height>0){const auto track=ii::gui::srgb_to_linear(c.palette.progress_track),fill=ii::gui::srgb_to_linear(c.palette.progress);
                drawing->panels.push_back({rect(g.progress_track),track,track,float(g.progress_track.height/2)});
                if(g.progress_fill.width>0)drawing->panels.push_back({rect(g.progress_fill),fill,fill,float(g.progress_fill.height/2)});
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_button(
            const Snapshot& value,const ii::gui::WidgetPaintState& state,bool menu) {
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& c=value->model.configuration();
            const auto background=state.pressed?c.palette.selected:state.hovered?c.palette.hover:c.palette.surface;
            const auto fill=ii::gui::srgb_to_linear(background),focus=ii::gui::srgb_to_linear(c.palette.focus);
            const auto radius=float((std::min)({6.,state.size.width/2,state.size.height/2}));
            const ii::gui::Rect bounds{0,0,float(state.size.width),float(state.size.height)};
            if(bounds.width>0&&bounds.height>0){drawing->panels.push_back({bounds,fill,fill,radius});
                if(state.enabled&&state.focused)drawing->panels.push_back({bounds,focus,focus,radius,1.5f});}
            const auto label=menu?std::string("⋯"):std::string(value->model.action_text());
            if(!label.empty()&&bounds.width>0&&bounds.height>0)drawing->texts.push_back({.bounds=bounds,.text=label,
                .color=ii::gui::srgb_to_linear(state.enabled?c.palette.text:c.palette.disabled_text),.font_size=menu?16.f:12.f,.font_weight=500,
                .alignment=ii::gui::TextAlignment::center,.font_family=state.font_family,.line_height=float(state.size.height),.elide_right=true,.clip=bounds});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card action paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state) {
        auto snapshot=state->prepared;if(state->retired||!snapshot)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");
        const auto g=snapshot->model.geometry(state->extent.width,state->extent.height);
        if(!g)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid card layout extent");
        if(auto r=state->surface.set_frame(rect(g->surface));!r)return r;
        const auto slot=[&](const std::optional<ViewSlot>& view,CardRect area)->ii::core::Result<void>{
            if(!view)return {};
            if(auto r=view->container.set_frame(rect(area));!r)return r;
            if(auto r=view->host.set_frame({0,0,float(area.width),float(area.height)});!r)return r;
            if(auto r=view->container.set_visible(area.width>0&&area.height>0);!r)return r;
            if(auto r=view->container.set_enabled(snapshot->authored.configuration.enabled);!r)return r;
            return view->container.set_opacity(snapshot->authored.configuration.enabled?1.0:0.4);
        };
        if(auto r=slot(state->content,g->surface);!r)return r;
        if(auto r=slot(state->preview,g->preview);!r)return r;
        if(auto r=slot(state->icon,g->icon);!r)return r;
        const bool menu=snapshot->model.shows_menu()&&g->menu.width>0&&g->menu.height>0;
        const bool action=snapshot->model.shows_action()&&g->action.width>0&&g->action.height>0;
        if(auto r=state->menu.set_visible(menu);!r)return r;if(menu)if(auto r=state->menu.set_frame(rect(g->menu));!r)return r;
        if(auto r=state->action.set_visible(action);!r)return r;if(action)if(auto r=state->action.set_frame(rect(g->action));!r)return r;
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state,std::uint64_t revision) {
        const auto step=[&](auto&& action)->ii::core::Result<bool>{if(state->retired||state->revision!=revision)return false;auto result=action();
            if(!result)return std::unexpected(result.error());return !state->retired&&state->revision==revision;};
        const auto size=state->prepared->model.intrinsic_size();
        auto result=step([&]{return state->root.set_extent({size.width,size.height});});if(!result||!*result)return result;
        result=step([&]{return state->root.set_visible(state->prepared->authored.configuration.visible);});if(!result||!*result)return result;
        result=step([&]{return state->surface.set_action_options(surface_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return state->menu.set_action_options(menu_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return state->action.set_action_options(action_options(state->prepared));});if(!result||!*result)return result;
        result=step([&]{return frames(state);});if(!result||!*result)return result;
        result=step([&]{return state->surface.request_repaint();});if(!result||!*result)return result;
        result=step([&]{return state->menu.request_repaint();});if(!result||!*result)return result;
        return step([&]{return state->action.request_repaint();});
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,CardInput input) {
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        try {
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());
            if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)()||state->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Card revision exhausted");
            const auto observed=++state->request_revision;if(state->prepared->authored==input)return {};
            auto next=prepare(input,*state);if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");
            if(state->request_revision!=observed||!state->live)return {};if(!next)return std::unexpected(next.error());
            auto previous=state->prepared;const auto old_extent=state->extent;const auto revision=++state->revision;state->prepared=std::move(*next);
            const auto size=state->prepared->model.intrinsic_size();state->extent={size.width,size.height};
            auto result=apply(state,revision);
            if(!result){if(!state->retired&&state->revision==revision){state->prepared=std::move(previous);state->extent=old_extent;(void)apply(state,revision);}return std::unexpected(result.error());}
            state->failure.reset();return {};
        } catch (...) {return ii::core::fail(ii::core::ErrorCode::failed,"Card update failed",std::current_exception());}
    }
    static void activate(const std::shared_ptr<State>& state) noexcept {
        try {
            if(!state->live || !state->prepared->authored.configuration.enabled || !state->prepared->authored.configuration.visible)return;
            const bool doubled=std::exchange(state->pending_double,false);
            auto model=state->prepared->model;const auto selected=model.toggle_selection();auto revision=state->revision;
            if(selected){auto input=state->prepared->authored;input.configuration.selected=*selected;auto changed=update(state,std::move(input));
                if(!changed){state->failure=changed.error();return;}if(!state->live||state->revision!=revision+1)return;++revision;
                if(state->callbacks.selected_changed){auto callback=state->callbacks.selected_changed;callback(*selected);}}
            if(state->live&&state->revision==revision&&state->callbacks.clicked){auto callback=state->callbacks.clicked;callback();}
            if(doubled&&state->live&&state->prepared->authored.configuration.enabled&&state->prepared->authored.configuration.visible&&state->callbacks.double_clicked){
                auto callback=state->callbacks.double_clicked;callback();}
        } catch (...) {try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Card activation failed",std::current_exception()};}catch(...) {}}
    }
    static void trigger(const std::shared_ptr<State>& state,bool menu) noexcept {
        try {if(!state->live||!state->prepared->authored.configuration.enabled||!state->prepared->authored.configuration.visible)return;
            auto callback=menu?state->callbacks.menu_requested:state->callbacks.action_triggered;if(callback)callback();
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Card action failed",std::current_exception()};}catch(...) {}}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeCard> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            CardInput input={},NativeCardCallbacks callbacks={},NativeCardViews views={}) {
        try {
            if(views.content&&(views.preview||views.icon))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Card content cannot be combined with partial content Views");
            auto state=std::make_shared<State>();state->custom_content=bool(views.content);state->custom_preview=bool(views.preview);state->custom_icon=bool(views.icon);
            auto prepared=prepare(input,*state);if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);const auto size=state->prepared->model.intrinsic_size();
            state->extent={size.width,size.height};std::weak_ptr<State> weak=state;const auto name=[&](std::string_view suffix){return std::string(id)+"."+std::string(suffix);};
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto surface=builder.action_surface(*root,name("surface"),surface_options(state->prepared),[weak]{if(auto state=weak.lock())activate(state);});
            if(!surface)return std::unexpected(surface.error());state->surface=*surface;
            if(auto events=surface->set_control_events({.canceled=[weak]{if(auto state=weak.lock()){
                state->pending_double=false;state->last_press=0;}}});!events)return std::unexpected(events.error());
            if(auto handler=surface->set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                auto state=weak.lock();if(!state||!state->live)return false;using K=ii::gui::WidgetInputKind;
                const auto p=event.local_position.value_or(ii::gui::WidgetPoint{event.x,event.y});
                if(event.kind==K::pointer_down){
                    if(event.buttons&&!(event.buttons&1)){state->pending_double=false;return false;}
                    const bool same_device=!state->last_device||!event.device_id||state->last_device==event.device_id;
                    const bool timed=event.native_click_count==0&&same_device&&std::isfinite(event.timestamp)&&event.timestamp>state->last_press&&
                        state->last_press>0&&event.timestamp-state->last_press<=.4&&std::hypot(p.x-state->last_x,p.y-state->last_y)<=4;
                    state->pending_double=event.native_click_count==2||timed;
                    state->last_press=state->pending_double?0:(std::isfinite(event.timestamp)&&event.timestamp>0?event.timestamp:0);
                    state->last_x=p.x;state->last_y=p.y;state->last_device=event.device_id;
                }else if(event.kind==K::pointer_up){if(p.x<0||p.y<0||p.x>=state->extent.width||p.y>=state->extent.height)state->pending_double=false;}
                else if(event.kind==K::pointer_cancel||event.kind==K::focus_lost||event.kind==K::key_cancel){state->pending_double=false;state->last_press=0;}
                return false;
            });!handler)return std::unexpected(handler.error());
            if(auto paint=surface->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state||state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_surface(state->prepared,view,state->custom_content,state->custom_preview,state->custom_icon);
            });!paint)return std::unexpected(paint.error());
            const auto add_view=[&](ii::gui::WidgetFactory factory,std::string_view suffix,std::optional<ViewSlot>& destination)->ii::core::Result<void>{
                if(!factory)return {};
                auto container=builder.absolute_stack(*root,name(std::string(suffix)+"-container"),{});if(!container)return std::unexpected(container.error());
                if(auto clipped=container->set_clip_content(true);!clipped)return clipped;
                auto host=builder.content_host(*container,name(suffix),{});if(!host)return std::unexpected(host.error());
                if(auto mounted=host->set_content(std::move(factory));!mounted)return mounted;
                destination=ViewSlot{*container,*host};return {};
            };
            if(auto mounted=add_view(std::move(views.content),"content",state->content);!mounted)return std::unexpected(mounted.error());
            if(auto mounted=add_view(std::move(views.preview),"preview",state->preview);!mounted)return std::unexpected(mounted.error());
            if(auto mounted=add_view(std::move(views.icon),"icon",state->icon);!mounted)return std::unexpected(mounted.error());
            auto menu=builder.action_surface(*root,name("menu"),menu_options(state->prepared),[weak]{if(auto state=weak.lock())trigger(state,true);});
            if(!menu)return std::unexpected(menu.error());state->menu=*menu;
            if(auto paint=menu->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state||state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_button(state->prepared,view,true);
            });!paint)return std::unexpected(paint.error());
            auto action=builder.action_surface(*root,name("action"),action_options(state->prepared),[weak]{if(auto state=weak.lock())trigger(state,false);});
            if(!action)return std::unexpected(action.error());state->action=*action;
            if(auto paint=action->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state||state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Card retired");return paint_button(state->prepared,view,false);
            });!paint)return std::unexpected(paint.error());
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            if(auto layout=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->retired||!state->prepared)return;
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};
                auto result=frames(state);if(!result)state->failure=result.error();
            }});!layout)return std::unexpected(layout.error());
            auto applied=apply(state,state->revision);if(!applied)return std::unexpected(applied.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->retired=true;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;state->callbacks={};state->prepared.reset();state->icons.reset();
                state->content.reset();state->preview.reset();state->icon.reset();}});!retired)return std::unexpected(retired.error());
            return BasicNativeCard(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Card construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget surface()const{return state_->surface;}
    [[nodiscard]] ii::gui::Widget menu()const{return state_->menu;}
    [[nodiscard]] ii::gui::Widget action()const{return state_->action;}
    [[nodiscard]] ii::core::Result<CardInput> options()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->authored;
    }
    [[nodiscard]] ii::core::Result<void> set_options(CardInput input)const{auto state=state_;return update(state,std::move(input));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Card requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

using NativeCard=BasicNativeCard<DefaultCardHostPolicy>;
} // namespace ii::gui
