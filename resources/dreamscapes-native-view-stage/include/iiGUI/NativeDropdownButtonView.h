#pragma once
#include <iiGUI/DropdownButtonModel.h>
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
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct DropdownInput final {
    DropdownConfiguration configuration;
    std::shared_ptr<const RasterImage> icon;
    std::string icon_name;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const DropdownInput&,const DropdownInput&)=default;
};
struct DropdownCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
struct DefaultDropdownHostPolicy final {
    using Model=DropdownButtonModel;using Input=DropdownInput;using Tone=ButtonTone;using Rect=ButtonRect;using Color=WidgetColor;
    using IconCache=ButtonIconCache;using Callbacks=DropdownCallbacks;using MethodRegistry=ButtonMethodRegistry;
    using Method=ButtonMethod;using Event=ButtonEvent;using TonePort=BorderlessTonePort;
    [[nodiscard]] static bool valid_input(const Input& input)noexcept{
        return !(input.icon&&!input.icon_name.empty())&&(!input.icon_source||!input.icon_name.empty())&&input.icon_revision>=0;
    }
    [[nodiscard]] static std::uint64_t source_revision(const Input& input)noexcept{
        return !input.icon_name.empty()&&input.icon_source?input.icon_source->revision():0;
    }
    [[nodiscard]] static bool method_active(const Widget& root){return DefaultButtonHostPolicy::method_active(root);}
    [[nodiscard]] static core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::optional<Tone> tone,std::string_view trigger){return DefaultButtonHostPolicy::invoke_methods(root,methods,enabled,tone,trigger);}
};
template<class Policy>
class BasicNativeDropdownButton final {
    using DropdownButtonModel=typename Policy::Model;using DropdownInput=typename Policy::Input;
    using ButtonTone=typename Policy::Tone;using ButtonRect=typename Policy::Rect;using StyleColor=typename Policy::Color;
    using IconCache=typename Policy::IconCache;using NativeDropdownCallbacks=typename Policy::Callbacks;
    using ButtonMethodRegistry=typename Policy::MethodRegistry;using ButtonMethod=typename Policy::Method;
    using ButtonEvent=typename Policy::Event;using BorderlessTonePort=typename Policy::TonePort;
    struct Prepared final {DropdownButtonModel view;std::shared_ptr<const ii::gui::RasterImage> icon;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        DropdownInput authored;
        std::shared_ptr<IconCache> icons=std::make_shared<IconCache>();
        Snapshot prepared;
        std::optional<ButtonTone> forced_tone;
        ii::gui::Widget root;
        NativeDropdownCallbacks callbacks;
        ButtonMethodRegistry methods;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,request_revision=0,source_revision=0;
        bool live=false,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeDropdownButton(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const DropdownInput& input,std::optional<ButtonTone> force,IconCache& icons){
        if(!Policy::valid_input(input))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Dropdown icon inputs");
        if(!DropdownButtonModel::validate(input.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Dropdown configuration");
        auto next=std::make_shared<Prepared>();auto effective=input.configuration;
        if(force&&effective.tone!=ButtonTone::disabled)effective.tone=*force;
        if(!next->view.set_configuration(std::move(effective)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Dropdown tone");
        next->icon=input.icon;
        if(!input.icon_name.empty()){
            auto icon=icons.resolve(input.icon_name,input.icon_source,input.configuration.icon_size);
            if(!icon)return std::unexpected(icon.error());next->icon=std::move(*icon);
        }
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->view.configuration();ii::gui::ActionOptions result;
        result.text=c.icon_mode?std::string{}:c.text;
        result.accessibility_name=c.accessibility_name.empty()?(c.text.empty()?"Menu":c.text):c.accessibility_name;
        result.enabled=c.enabled;result.focus_ring=false;result.font_size=c.font_size;result.line_height=c.line_height;
        result.padding={c.left_padding+(c.icon_mode?c.icon_size:0),0,c.right_padding+c.indicator_size+c.spacing,0};
        result.width=c.width;result.height=c.height;result.interaction_enabled=c.tone!=ButtonTone::disabled;return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot,const ii::gui::WidgetPaintState& input){
        try{
            const auto& c=snapshot->view.configuration();const auto visual=snapshot->view.visual({input.enabled,input.hovered,input.pressed,input.focused});
            double text_width=0,text_height=0;
            if(!c.icon_mode&&!c.text.empty()){
                auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                    .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
                if(!measured)return std::unexpected(measured.error());text_width=(std::min)(8192.,measured->width);text_height=(std::min)(8192.,measured->height);
            }
            const auto geometry=snapshot->view.geometry(input.size.width,input.size.height,text_width,text_height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Dropdown extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();
            const auto rect=[](ButtonRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            if(geometry->surface.width>0&&geometry->surface.height>0){
                if(visual.background.alpha>0){const auto ink=linear(visual.background);result->panels.push_back({rect(geometry->surface),ink,ink,float(geometry->radius)});}
                if(visual.focused){const auto ink=linear(visual.focus);const auto stroke=float((std::min)({1.5,input.size.width/2,input.size.height/2}));
                    if(stroke>0)result->panels.push_back({rect(geometry->surface),ink,ink,float(geometry->radius),stroke});}
            }
            const ii::gui::Rect clip{0,0,float(input.size.width),float(input.size.height)};
            if(geometry->label.width>0&&geometry->label.height>0&&!c.text.empty())
                result->texts.push_back({.bounds=rect(geometry->label),.text=c.text,.color=linear(visual.text),.font_size=float(c.font_size),
                    .font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,.font_family=input.font_family,
                    .line_height=float(c.line_height),.elide_right=true,.clip=clip});
            if(geometry->icon.width>0&&geometry->icon.height>0){
                if(snapshot->icon)result->images.push_back({.bounds=rect(geometry->icon),.image=snapshot->icon,.opacity=visual.enabled?1.f:.3f});
                else{
                    // Default icon: an outlined folder with three accent tiles.
                    const auto& r=geometry->icon;const auto scale=float(r.width/18);const auto x=float(r.x),y=float(r.y);
                    auto ink=linear(visual.text),accent=linear(c.palette.primary);if(!visual.enabled)accent.alpha*=.3f;
                    if(scale>0){result->panels.push_back({{x+scale,y+4*scale,14*scale,10*scale},ink,ink,scale,scale});
                        result->panels.push_back({{x+2*scale,y+2*scale,5*scale,2*scale},ink,ink,0});
                        for(auto [dx,dy]:{std::pair{10,9},std::pair{13,9},std::pair{13,12}})
                            result->panels.push_back({{x+dx*scale,y+dy*scale,2*scale,2*scale},accent,accent,.4f*scale});}
                }
            }
            if(geometry->indicator.width>0&&geometry->indicator.height>0){
                ii::gui::TextRun glyph{rect(geometry->indicator),"⌄",linear(visual.text),float((std::min)(13.,geometry->indicator.height)),
                    400,ii::gui::TextAlignment::center,input.font_family};
                auto metrics=ii::gui::measure_text_layout(glyph);if(!metrics)return std::unexpected(metrics.error());
                glyph.bounds.y+=float((std::max)(0.,(geometry->indicator.height-metrics->height)/2));
                glyph.bounds.height=float((std::min)(geometry->indicator.height,metrics->height));glyph.clip=clip;
                if(glyph.bounds.height>0)result->texts.push_back(std::move(glyph));
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> apply(std::shared_ptr<State> state,DropdownInput input,std::optional<ButtonTone> force){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        try{
            if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
            if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Dropdown has retired");
            if(state->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Dropdown request revision exhausted");
            const auto request=++state->request_revision;const auto current_source=Policy::source_revision(input);
            if(state->retired||state->request_revision!=request)return {};
            if(state->authored==input&&state->forced_tone==force&&state->source_revision==current_source)return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Dropdown revision exhausted");
            if(!state->icons||(!input.icon_name.empty()&&state->authored.icon_revision!=input.icon_revision))
                state->icons=std::make_shared<IconCache>();
            auto icons=state->icons;std::optional<Snapshot> next;std::uint64_t source_revision=0;
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto before=Policy::source_revision(input);auto candidate=prepare(input,force,*icons);
                if(state->retired)return {};
                if(state->request_revision!=request){if(state->icons==icons)state->icons.reset();return {};}
                if(Policy::source_revision(input)!=before)continue;
                if(state->retired)return {};
                if(state->request_revision!=request){if(state->icons==icons)state->icons.reset();return {};}
                if(!candidate)return std::unexpected(candidate.error());
                next=std::move(*candidate);source_revision=before;break;
            }
            if(!next){if(state->icons==icons)state->icons.reset();return ii::core::fail(ii::core::ErrorCode::busy,"Dropdown icon source changed repeatedly");}
            auto previous=state->authored;auto old_snapshot=state->prepared;auto old_force=state->forced_tone;
            const auto old_source_revision=state->source_revision;const auto revision=++state->revision;
            state->authored=std::move(input);state->prepared=std::move(*next);state->forced_tone=force;state->source_revision=source_revision;
            if(auto r=state->root.set_action_options(action(state->prepared));!r){
                if(!state->retired&&state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old_snapshot);state->forced_tone=old_force;state->source_revision=old_source_revision;}return r;
            }
            if(state->retired||state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown update failed",std::current_exception());}
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeDropdownButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,DropdownInput input={},NativeDropdownCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();std::optional<Snapshot> prepared;std::uint64_t source_revision=0;
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto before=Policy::source_revision(input);auto candidate=prepare(input,std::nullopt,*state->icons);
                if(Policy::source_revision(input)!=before)continue;
                if(!candidate)return std::unexpected(candidate.error());prepared=std::move(*candidate);source_revision=before;break;
            }
            if(!prepared)return ii::core::fail(ii::core::ErrorCode::busy,"Dropdown icon source changed repeatedly");
            state->authored=std::move(input);state->prepared=std::move(*prepared);state->source_revision=source_revision;state->callbacks=std::move(callbacks);
            std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member){return [weak,member]{if(auto state=weak.lock();state&&state->live){auto callback=state->callbacks.*member;if(callback)callback();}};};
            auto root=builder.action_surface(parent,id,action(state->prepared),[weak]{
                auto state=weak.lock();if(!state||!state->live||!Policy::method_active(state->root))return;
                const auto& c=state->authored.configuration;
                const ButtonEvent value{state->root,"clicked",c.enabled,true,c.tone};
                if(!state->methods.invoke(value,[&]{return state->live&&Policy::method_active(state->root);}))return;
                auto callback=state->callbacks.clicked;if(callback)callback();
            });if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_control_events({.pressed=event(&NativeDropdownCallbacks::pressed),.released=event(&NativeDropdownCallbacks::released),.canceled=event(&NativeDropdownCallbacks::canceled)});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state->prepared,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){
                    state->live=false;state->retired=true;state->prepared.reset();state->icons.reset();
                    state->authored=DropdownInput{};state->callbacks={};state->methods=ButtonMethodRegistry{};
                }});!r)return std::unexpected(r.error());
            return BasicNativeDropdownButton(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<bool> has_methods()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());state_->methods.set_method(std::move(method));return {};
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.set_methods(std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="clicked")const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        const auto& c=state_->authored.configuration;return Policy::invoke_methods(state_->root,state_->methods,c.enabled,c.tone,trigger);
    }
    [[nodiscard]] ii::core::Result<DropdownInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(DropdownInput value)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        return apply(state_,std::move(value),state_->forced_tone);
    }
    [[nodiscard]] BorderlessTonePort weak_borderless_tone_port()const{
        std::weak_ptr<State> weak=state_;
        return {.apply=[weak](bool force)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Dropdown has retired");
                    if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
                    return apply(state,state->authored,force?std::optional<ButtonTone>{ButtonTone::borderless}:std::nullopt);},
                .retired=[weak]{auto state=weak.lock();if(!state)return true;if(state->owner!=std::this_thread::get_id())return false;
                    auto current=state->root.snapshot();return !current&&current.error().code==ii::core::ErrorCode::closed;}};
    }
};
using NativeDropdownButton=BasicNativeDropdownButton<DefaultDropdownHostPolicy>;
} // namespace ii::gui
