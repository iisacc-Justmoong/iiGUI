#pragma once
#include <iiGUI/TabModel.h>
#include <iiGUI/NativeButtonView.h>
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
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
struct TabInput final {TabConfiguration configuration;std::shared_ptr<const ButtonIconSource> icon_source;std::int64_t icon_revision=0;bool visible=true;
 friend bool operator==(const TabInput&,const TabInput&)=default;};
struct TabPresentation final {std::optional<bool> selected;std::optional<double> width;friend bool operator==(const TabPresentation&,const TabPresentation&)=default;};
struct TabAvailability final {bool enabled=true,visible=true;};
struct TabGroupPort final {
 std::function<core::Result<void>(TabPresentation)> present;std::function<core::Result<void>(std::function<bool()>)> activated;
 std::function<core::Result<void>(std::function<void()>)> changed;std::function<core::Result<TabAvailability>()> availability;
 std::function<core::Result<double>()> minimum_width;std::function<core::Result<WidgetSnapshot>()> snapshot;
 std::function<core::Result<void>()> focus;std::function<bool()> retired;std::function<core::Result<Widget>()> widget;std::vector<Widget> layout_groups;
};
struct DefaultTabHostPolicy final {using Model=TabModel;using Input=TabInput;using Presentation=TabPresentation;using GroupPort=TabGroupPort;
 using Availability=TabAvailability;using Rect=TabRect;using Color=WidgetColor;using IconCache=ButtonIconCache;
 [[nodiscard]] static std::uint64_t source_revision(const Input& value)noexcept{return value.icon_source?value.icon_source->revision():0;}};
template<class Policy>class BasicNativeTab final {
    using Tab=typename Policy::Model;using TabInput=typename Policy::Input;using TabPresentation=typename Policy::Presentation;
    using TabGroupPort=typename Policy::GroupPort;using TabAvailability=typename Policy::Availability;
    using TabRect=typename Policy::Rect;using StyleColor=typename Policy::Color;using IconCache=typename Policy::IconCache;
    struct Prepared final {Tab model;std::shared_ptr<const ii::gui::RasterImage> icon;double badge_width=0;std::optional<double> width;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        TabInput authored;
        std::shared_ptr<IconCache> icons=std::make_shared<IconCache>();
        Snapshot prepared;
        TabPresentation presentation;
        ii::gui::Widget root;
        ii::runtime::Task clicked;
        std::function<bool()> group_activation;
        std::function<void()> group_changed;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,source_revision=0,request_revision=0;
        bool retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeTab(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const TabInput& input,TabPresentation presentation,IconCache& icons){
        if(input.icon_revision<0||!Tab::validate(input.configuration)||
           (presentation.width&&(!std::isfinite(*presentation.width)||*presentation.width<1||*presentation.width>8192)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Tab configuration");
        auto next=std::make_shared<Prepared>();auto effective=input.configuration;
        if(presentation.selected)effective.selected=*presentation.selected;
        (void)next->model.set_configuration(std::move(effective));next->width=presentation.width;
        auto image=icons.resolve(input.configuration.icon_name,input.icon_source,input.configuration.icon_size);
        if(!image)return std::unexpected(image.error());next->icon=std::move(*image);
        if(!input.configuration.badge.empty()){
            auto measure=ii::gui::measure_text_layout({.text=input.configuration.badge,.font_size=float(input.configuration.font_size),
                .font_weight=input.configuration.font_weight,.line_height=float(input.configuration.line_height),.elide_right=true});
            if(!measure)return std::unexpected(measure.error());next->badge_width=(std::min)(8192.,measure->width);
        }
        const auto& c=input.configuration;
        if(c.horizontal_padding+c.icon_size+c.spacing>8192||c.horizontal_padding+next->badge_width+(next->badge_width?c.spacing:0)>8192)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"LVUI Tab content padding exceeds host limits");
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->model.configuration();
        return {.text=c.text,.accessibility_name=c.accessibility_name.empty()?(c.text.empty()?c.icon_name:c.text):c.accessibility_name,
                .enabled=c.enabled,.focus_ring=false,.role=ii::gui::ActionRole::tab,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.horizontal_padding+c.icon_size+(c.text.empty()?0:c.spacing),0,
                          c.horizontal_padding+value->badge_width+(value->badge_width?c.spacing:0),0},
                .minimum_width=c.minimum_width,.width=value->width,.height=c.height,.checked=c.selected};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const Snapshot& snapshot,const ii::gui::WidgetPaintState& state){
        try{
            const auto& c=snapshot->model.configuration();const auto visual=snapshot->model.visual({state.enabled,state.hovered,state.pressed,state.focused});
            const auto measure=[&](const std::string& text)->ii::core::Result<ii::gui::TextMetrics>{
                return ii::gui::measure_text_layout({.text=text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                    .font_family=state.font_family,.line_height=float(c.line_height),.elide_right=true});};
            double lw=0,lh=0,bw=0,bh=0;
            if(!c.text.empty()){auto m=measure(c.text);if(!m)return std::unexpected(m.error());lw=(std::min)(32767.,m->width);lh=(std::min)(32767.,m->height);}
            if(!c.badge.empty()){auto m=measure(c.badge);if(!m)return std::unexpected(m.error());bw=(std::min)(32767.,m->width);bh=(std::min)(32767.,m->height);}
            const auto layout=snapshot->model.geometry(state.size.width,state.size.height,lw,lh,bw,bh);
            if(!layout)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Tab paint extent");
            const auto rect=[](TabRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            auto result=std::make_shared<ii::gui::WidgetDrawing>();
            if(visual.background.alpha>0&&layout->surface.width>0&&layout->surface.height>0){const auto ink=linear(visual.background);
                result->panels.push_back({rect(layout->surface),ink,ink,float(layout->radius)});}
            if(visual.underline&&layout->underline.width>0&&layout->underline.height>0){const auto ink=linear(visual.accent);
                result->panels.push_back({rect(layout->underline),ink,ink,0});}
            if(visual.focused&&layout->surface.width>0&&layout->surface.height>0){const auto ink=linear(visual.accent);
                result->panels.push_back({rect(layout->surface),ink,ink,float(layout->radius),float((std::min)({2.,layout->surface.width/2,layout->surface.height/2}))});}
            if(layout->icon.width>0&&layout->icon.height>0)
                result->images.push_back({.bounds=rect(layout->icon),.image=snapshot->icon,.opacity=c.enabled&&state.enabled?1.f:.3f});
            const ii::gui::Rect clip{0,0,float(state.size.width),float(state.size.height)};
            if(layout->label.width>0&&layout->label.height>0)result->texts.push_back({.bounds=rect(layout->label),.text=c.text,.color=linear(visual.text),
                .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,.font_family=state.font_family,
                .line_height=float(c.line_height),.elide_right=true,.clip=clip});
            if(layout->badge.width>0&&layout->badge.height>0)result->texts.push_back({.bounds=rect(layout->badge),.text=c.badge,.color=linear(visual.badge),
                .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,.font_family=state.font_family,
                .line_height=float(c.line_height),.elide_right=true,.clip=clip});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Tab paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTab> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const TabInput& input,ii::runtime::Task clicked={}){
        try{
            auto state=std::make_shared<State>();auto prepared=prepare(input,{},*state->icons);if(!prepared)return std::unexpected(prepared.error());
            state->authored=input;state->prepared=std::move(*prepared);state->source_revision=Policy::source_revision(input);state->clicked=std::move(clicked);
            std::weak_ptr<State> weak=state;
            auto root=builder.action_surface(parent,id,action(state->prepared),[weak]{
                if(auto state=weak.lock()){
                    auto group=state->group_activation;if(group&&!group())return;
                    if(!state->root.snapshot())return;auto clicked=state->clicked;if(clicked)clicked();
                }
            });if(!root)return std::unexpected(root.error());
            state->root=*root;if(!input.visible)if(auto r=root->set_visible(false);!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");return paint(state->prepared,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->authored.icon_source.reset();state->prepared.reset();
                state->icons.reset();state->clicked={};state->group_activation={};state->group_changed={};}});!r)return std::unexpected(r.error());
            return BasicNativeTab(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Tab creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TabInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tab options failed",std::current_exception());}
    }
private:
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,TabInput input,TabPresentation presentation){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
        try{
            if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)()||state->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Tab revision exhausted");
            const auto observed=++state->request_revision;const auto current=Policy::source_revision(input);
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
            if(state->request_revision!=observed)return {};
            if(input==state->authored&&presentation==state->presentation&&current==state->source_revision)return {};
            auto icons=state->icons;if(!icons)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
            std::optional<Snapshot> prepared;std::uint64_t source_revision=0;
            for(unsigned attempt=0;attempt<4;++attempt){const auto before=Policy::source_revision(input);
                auto candidate=prepare(input,presentation,*icons);const auto after=Policy::source_revision(input);
                if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->request_revision!=observed)return {};if(before!=after)continue;if(!candidate)return std::unexpected(candidate.error());
                prepared=std::move(*candidate);source_revision=after;break;}
            if(!prepared)return ii::core::fail(ii::core::ErrorCode::busy,"Tab icon source changed repeatedly");
            auto previous=state->authored;auto old=state->prepared;auto old_presentation=state->presentation;auto old_source=state->source_revision;const auto revision=++state->revision;
            state->authored=input;state->prepared=std::move(*prepared);state->presentation=presentation;state->source_revision=source_revision;
            if(auto changed=state->root.set_action_options(action(state->prepared));!changed){
                if(!state->retired&&state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old);state->presentation=old_presentation;state->source_revision=old_source;}return changed;
            }
            if(state->retired||state->revision!=revision)return {};
            if(previous.visible!=input.visible)if(auto changed=state->root.set_visible(input.visible);!changed){
                if(!state->retired&&state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old);state->presentation=old_presentation;state->source_revision=old_source;
                    (void)state->root.set_action_options(action(state->prepared));}
                return changed;
            }
            if(state->retired||state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Tab update failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_options(const TabInput& input)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
        auto state=state_;const auto observed=state->revision;auto result=apply(state,input,state->presentation);if(!result)return result;
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
        try{auto notify=state->group_changed;if(state->revision!=observed&&notify)notify();return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Tab group notification failed",std::current_exception());}
    }
    [[nodiscard]] TabGroupPort weak_group_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .present=[weak](TabPresentation value)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");return apply(state,state->authored,value);},
            .activated=[weak](std::function<bool()> hook)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(hook&&state->group_activation)return ii::core::fail(ii::core::ErrorCode::already_exists,"Tab already belongs to a group");
                state->group_activation=std::move(hook);return {};},
            .changed=[weak](std::function<void()> hook)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(hook&&state->group_changed)return ii::core::fail(ii::core::ErrorCode::already_exists,"Tab already has a group observer");
                state->group_changed=std::move(hook);return {};},
            .availability=[weak]()->ii::core::Result<TabAvailability>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                return TabAvailability{state->authored.configuration.enabled,state->authored.visible};},
            .minimum_width=[weak]()->ii::core::Result<double>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return state->authored.configuration.minimum_width;},
            .snapshot=[weak]()->ii::core::Result<ii::gui::WidgetSnapshot>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");return state->root.snapshot();},
            .focus=[weak]()->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");return state->root.focus();},
            .retired=[weak]{auto state=weak.lock();if(!state)return true;if(state->owner!=std::this_thread::get_id())return false;
                auto snapshot=state->root.snapshot();return !snapshot&&snapshot.error().code==ii::core::ErrorCode::closed;},
            .widget=[weak]()->ii::core::Result<ii::gui::Widget>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Tab has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Tab requires its owner thread");
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return state->root;}
        };
    }
};
using NativeTab=BasicNativeTab<DefaultTabHostPolicy>;
} // namespace ii::gui
