#pragma once
#include <iiGUI/ButtonModel.h>
#include <iiGUI/WidgetMotion.h>
#include <iiGUI/Motion.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ii::gui {
// Icon pixels are supplied by the application; iiGUI owns their placement.
class ButtonIconSource {
public:
    virtual ~ButtonIconSource()=default;
    [[nodiscard]] virtual std::uint64_t revision() const noexcept { return 0; }
    [[nodiscard]] virtual core::Result<std::shared_ptr<const RasterImage>> resolve(std::string_view,double) const=0;
};
struct ButtonTiming final {
    std::chrono::milliseconds hover{},press{},release{},post_release{},programmatic{},focus_opacity{},focus_scale{};
    friend bool operator==(const ButtonTiming&,const ButtonTiming&)=default;
};
struct ButtonInput final {
    std::shared_ptr<const RasterImage> icon;
    ButtonConfiguration configuration;
    std::string icon_name;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    ButtonTiming timing;
    friend bool operator==(const ButtonInput&,const ButtonInput&)=default;
};
struct NativeButtonEvents final { runtime::Task pressed,released,canceled; };
struct ButtonEvent final {
    Widget source;
    std::string trigger="clicked";
    bool enabled=true,effective_enabled=true;
    std::optional<ButtonTone> tone;
};
using ButtonMethod=std::function<void(const ButtonEvent&)>;
struct BorderlessTonePort final {
    std::function<core::Result<void>(bool)> apply;
    std::function<bool()> retired;
};
class ButtonIconCache final {
    std::shared_ptr<const ButtonIconSource> source_;
    std::string name_;
    double size_=0;
    std::uint64_t revision_=0;
    std::shared_ptr<const RasterImage> image_;
public:
    [[nodiscard]] core::Result<std::shared_ptr<const RasterImage>> resolve(
            std::string_view name,std::shared_ptr<const ButtonIconSource> source,double size) {
        if(!source||name.empty()||name.size()>256||name.find('\0')!=std::string_view::npos||
           !ii::core::valid_utf8(name)||!std::isfinite(size)||size<1||size>256)
            return core::fail(core::ErrorCode::invalid_argument,"Invalid button icon request");
        try {
            for(unsigned attempt=0;attempt<4;++attempt) {
                const auto revision=source->revision();
                if(source_==source && name_==name && size_==size && revision_==revision && image_) return image_;
                auto image=source->resolve(name,size);
                if(source->revision()!=revision) continue;
                if(!image) return std::unexpected(image.error());
                if(!*image) return core::fail(core::ErrorCode::invalid_argument,"Missing button icon image");
                auto next_name=std::string(name);
                auto next_image=*image;
                source_=std::move(source);name_=std::move(next_name);size_=size;revision_=revision;image_=std::move(next_image);
                return image_;
            }
            return core::fail(core::ErrorCode::busy,"Button icon changed repeatedly during resolution");
        } catch(...) { return core::fail(core::ErrorCode::failed,"Button icon resolution failed",std::current_exception()); }
    }
};
class ButtonMethodRegistry final {
    ButtonMethod method_;
    std::vector<ButtonMethod> methods_;
public:
    [[nodiscard]] bool has_methods()const noexcept { return bool(method_)||std::ranges::any_of(methods_,[](const auto& f){return bool(f);}); }
    void set_method(ButtonMethod method)noexcept { method_=std::move(method); }
    [[nodiscard]] core::Result<void> set_methods(std::vector<ButtonMethod> methods) {
        std::erase_if(methods,[](const auto& f){return !f;});
        if(methods.size()>256)return core::fail(core::ErrorCode::invalid_argument,"Too many Button methods");
        methods_=std::move(methods);return {};
    }
    [[nodiscard]] core::Result<void> set_registry(ButtonMethod method,std::vector<ButtonMethod> methods) {
        std::erase_if(methods,[](const auto& f){return !f;});
        if(methods.size()>256)return core::fail(core::ErrorCode::invalid_argument,"Too many Button methods");
        method_=std::move(method);methods_=std::move(methods);return {};
    }
    template<class Active> [[nodiscard]] bool invoke(const ButtonEvent& event,Active&& active)const {
        auto method=method_;auto methods=methods_;
        if(method){if(!active())return false;method(event);}
        for(auto& item:methods)if(item){if(!active())return false;item(event);}
        return active();
    }
};
struct DefaultButtonHostPolicy final {
    using Model=ButtonModel; using Input=ButtonInput; using Tone=ButtonTone;
    using Rect=ButtonRect; using Color=WidgetColor; using IconCache=ButtonIconCache;
    using Events=NativeButtonEvents; using Event=ButtonEvent; using Method=ButtonMethod;
    using MethodRegistry=ButtonMethodRegistry; using TonePort=BorderlessTonePort;
    [[nodiscard]] static bool method_active(const Widget& root) {
        auto interaction=root.interaction();
        if(!interaction||!interaction->alive||!interaction->enabled||!interaction->visible||interaction->input_blocked)return false;
        auto action=root.action_options();return action&&action->enabled&&action->interaction_enabled;
    }
    [[nodiscard]] static core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::optional<Tone> tone,std::string_view trigger) {
        auto snapshot=root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        bool effective=snapshot->enabled;
        if(tone){auto options=root.action_options();if(!options)return std::unexpected(options.error());effective=effective&&options->enabled&&options->interaction_enabled;}
        try {
            const Event event{root,std::string(trigger.empty()?"clicked":trigger),enabled,effective,tone};
            (void)methods.invoke(event,[&]{return root.snapshot().has_value();});return {};
        } catch(...) {return core::fail(core::ErrorCode::failed,"Button methods failed",std::current_exception());}
    }
    [[nodiscard]] static auto hover_duration(const Input& i) { return i.timing.hover; }
    [[nodiscard]] static auto press_duration(const Input& i) { return i.timing.press; }
    [[nodiscard]] static auto release_duration(const Input& i) { return i.timing.release; }
    [[nodiscard]] static auto post_release_duration(const Input& i) { return i.timing.post_release; }
    [[nodiscard]] static auto programmatic_duration(const Input& i) { return i.timing.programmatic; }
    [[nodiscard]] static auto focus_opacity_duration(const Input& i) { return i.timing.focus_opacity; }
    [[nodiscard]] static auto focus_scale_duration(const Input& i) { return i.timing.focus_scale; }
};
// Policy supplies values and callback adapters; rendering, interaction,
// lifetime and frame admission belong to iiGUI.
template<class Policy>
class BasicNativeButton final {
    using Button = typename Policy::Model;
    using ButtonInput = typename Policy::Input;
    using ButtonTone = typename Policy::Tone;
    using ButtonRect = typename Policy::Rect;
    using StyleColor = typename Policy::Color;
    using NativeIconCache = typename Policy::IconCache;
    using NativeButtonEvents = typename Policy::Events;
    using ButtonEvent = typename Policy::Event;
    using ButtonMethod = typename Policy::Method;
    using ButtonMethodRegistry = typename Policy::MethodRegistry;
    using BorderlessTonePort = typename Policy::TonePort;
    struct Prepared final { Button view; std::shared_ptr<const ii::gui::RasterImage> icon; };
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        ButtonInput authored;
        NativeIconCache icons;
        Snapshot prepared;
        std::optional<ButtonTone> forced_tone;
        ii::gui::Widget root;
        NativeButtonEvents events;
        ButtonMethodRegistry methods;
        WidgetMotionTransition<4> background;
        WidgetMotionTransition<1> focus_opacity;
        WidgetMotionTransition<1> focus_scale{{.96}};
        std::shared_ptr<ii::gui::MotionPreferences> motion;
        ii::gui::WidgetFrameHandler frame;
        ii::gui::WidgetInteraction last_interaction;
        std::optional<ii::runtime::Clock::time_point> last_frame_time,release_started;
        ii::runtime::Clock::duration release_window{};
        std::chrono::milliseconds background_duration{};
        WidgetMotionCurve background_curve=WidgetMotionCurve::out_cubic;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        std::uint64_t motion_revision=0;
        unsigned options_depth=0;
        bool live=false,interaction_seen=false,pointer_departed=false,last_effective_pressed=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeButton(std::shared_ptr<State> state):state_(std::move(state)){}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ButtonInput& input,NativeIconCache& icons){
        if((input.icon&&!input.icon_name.empty())||(input.icon_source&&input.icon_name.empty())||input.icon_revision<0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Conflicting Button icon inputs");
        const auto valid_duration=[](std::chrono::milliseconds value){
            return value>=std::chrono::milliseconds::zero() && value<=std::chrono::hours(24);
        };
        if(!valid_duration(Policy::hover_duration(input)) || !valid_duration(Policy::press_duration(input)) ||
           !valid_duration(Policy::release_duration(input)) || !valid_duration(Policy::post_release_duration(input)) ||
           !valid_duration(Policy::programmatic_duration(input)) || !valid_duration(Policy::focus_opacity_duration(input)) ||
           !valid_duration(Policy::focus_scale_duration(input)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button timing");
        auto next=std::make_shared<Prepared>();
        auto value=input.configuration;value.has_icon=bool(input.icon)||!input.icon_name.empty();
        if(!next->view.set_configuration(std::move(value)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button configuration");
        next->icon=input.icon;
        if(!input.icon_name.empty()){
            auto icon=icons.resolve(input.icon_name,input.icon_source,input.configuration.icon_size);
            if(!icon)return std::unexpected(icon.error());next->icon=std::move(*icon);
        }
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const Snapshot& snapshot){
        const auto& value=snapshot->view.configuration();
        const auto icon_space=value.icon_mode?value.icon_size+snapshot->view.icon_gap():0;
        return {.text=value.text,.accessibility_name=value.accessibility_name,.enabled=value.enabled,
                .focus_ring=false,.role=ii::gui::ActionRole::button,.text_role=ii::gui::TextRole::body,
                .font_size=value.font_size,.line_height=value.line_height,
                .padding={value.horizontal_padding+icon_space,0,value.horizontal_padding,0},
                .width=value.width,.height=value.height,.interaction_enabled=value.tone!=ButtonTone::disabled,
                .checkable=value.checkable,.checked=value.checked,.enter_key_activation=value.enter_key_activation};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& owner,const ii::gui::WidgetPaintState& state){
        const auto& snapshot=owner->prepared;
        const auto& value=snapshot->view.configuration();
        auto visual=snapshot->view.visual({state.enabled,state.hovered,
            state.pressed&&!owner->pointer_departed,state.focused,state.checked});
        // Direct host mutations of checked state do not have an interaction
        // notification. Show the current model color rather than a stale
        // transition; ordinary input and set_options retarget it explicitly.
        const auto expected=channels(visual.background);
        if(owner->background.target()==expected)visual.background=color(owner->background.value());
        double text_width=0,text_height=0;
        if(!value.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=value.text,.font_size=float(value.font_size),
                .font_weight=value.font_weight,.font_family=state.font_family,.line_height=float(value.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());
            text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=snapshot->view.geometry(state.size.width,state.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button extent");
        const auto rect=[](ButtonRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(v.red)),float(channel(v.green)),float(channel(v.blue)),float(v.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(geometry->surface.width>0&&geometry->surface.height>0){
            if(visual.background.alpha>0){const auto color=linear(visual.background);drawing->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius)});}
            if(value.border_color&&value.border_width>0){const auto color=linear(*value.border_color);
                const auto stroke=(std::min)({value.border_width,geometry->surface.width/2,geometry->surface.height/2});
                if(color.alpha>0&&stroke>0)drawing->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius),float(stroke)});}
            const auto opacity=owner->live?std::clamp(owner->focus_opacity.value()[0],0.,1.):(visual.focused?1.:0.);
            if(opacity>0){auto color=linear(visual.focus);color.alpha*=float(opacity);
                // Keep the focus indication outside the hit target, while
                // leaving ancestor clips and layout geometry unchanged.
                const auto x=(std::min)(3.,(std::max)(0.,(8192.-geometry->surface.width)/2));
                const auto y=(std::min)(3.,(std::max)(0.,(8192.-geometry->surface.height)/2));
                const ButtonRect base{-x,-y,geometry->surface.width+2*x,geometry->surface.height+2*y};
                const auto scale=std::clamp(owner->live?owner->focus_scale.value()[0]:1.,0.,(std::min)(8192./base.width,8192./base.height));
                const ButtonRect focus{base.x+base.width*(1-scale)/2,base.y+base.height*(1-scale)/2,
                    base.width*scale,base.height*scale};
                const auto stroke=(std::min)({1.5*scale,focus.width/2,focus.height/2});
                const auto radius=(std::min)({(geometry->radius+(std::min)(x,y))*scale,focus.width/2,focus.height/2});
                if(stroke>0)drawing->panels.push_back({rect(focus),color,color,float(radius),float(stroke)});}
        }
        if(geometry->draw_icon&&snapshot->icon)
            drawing->images.push_back({.bounds=rect(geometry->icon),.image=snapshot->icon,.opacity=visual.enabled?1.f:.3f});
        if(geometry->draw_text)
            drawing->texts.push_back({.bounds=rect(geometry->text),.text=value.text,.color=linear(visual.text),
                .font_size=float(value.font_size),.font_weight=value.font_weight,.alignment=ii::gui::TextAlignment::center,
                .font_family=state.font_family,.line_height=float(value.line_height),.elide_right=true,
                .clip=ii::gui::Rect{0,0,float(state.size.width),float(state.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static std::array<double,4> channels(StyleColor value)noexcept{
        return {value.red,value.green,value.blue,value.alpha};
    }
    [[nodiscard]] static StyleColor color(const std::array<double,4>& value)noexcept{
        return {value[0],value[1],value[2],value[3]};
    }
    [[nodiscard]] static ii::core::Result<WidgetMotion> motion_policy(const std::shared_ptr<State>& state,
            std::chrono::milliseconds base,WidgetMotionCurve curve){
        auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
        auto duration=state->motion->duration(base);if(!duration)return std::unexpected(duration.error());
        state->motion_revision=preferences->revision;
        return WidgetMotion{.duration=std::chrono::duration_cast<std::chrono::milliseconds>(*duration),.curve=curve};
    }
    [[nodiscard]] static ii::core::Result<void> synchronize_displayed(const std::shared_ptr<State>& state,bool checked){
        if(!state->interaction_seen)return {};
        const auto& previous=state->last_interaction;
        const auto visible=state->prepared->view.visual({previous.enabled,previous.hovered,
            state->last_effective_pressed,previous.focused,checked});
        const auto expected=channels(visible.background);
        if(state->background.target()!=expected&&!state->background.reset(expected))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button color");
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> transition(const std::shared_ptr<State>& state,
            const ii::gui::WidgetInteraction& interaction,bool reconcile_external){
        using namespace std::chrono_literals;
        auto action=state->root.action_options();if(!action)return std::unexpected(action.error());
        if(reconcile_external&&!state->options_depth){
            if(auto synced=synchronize_displayed(state,action->checked);!synced)return synced;
        }
        auto origin=state->root.press_origin();if(!origin)return std::unexpected(origin.error());
        if(!interaction.pressed||*origin!=ii::gui::WidgetPressOrigin::pointer)state->pointer_departed=false;
        else if(!interaction.hovered)state->pointer_departed=true;
        const bool effective_pressed=interaction.pressed&&!state->pointer_departed;
        const auto visual=state->prepared->view.visual({interaction.enabled,interaction.hovered,
            effective_pressed,interaction.focused,action->checked});
        const auto target=channels(visual.background);
        const std::array<double,1> focus_opacity{visual.focused?1.:0.};
        const std::array<double,1> focus_scale{visual.focused?1.:.96};
        const auto clock_now=ii::runtime::Clock::now();
        const auto now=state->last_frame_time?(std::max)(clock_now,*state->last_frame_time):clock_now;
        const auto& previous=state->last_interaction;
        const bool previous_pressed=state->last_effective_pressed;
        const bool input_change=state->interaction_seen&&!state->options_depth&&
            previous.enabled&&interaction.enabled&&
            (previous_pressed!=effective_pressed||previous.hovered!=interaction.hovered);
        if(!interaction.enabled||!interaction.visible||!state->prepared->view.configuration().motion_enabled){
            state->release_started.reset();
        }else if(input_change){
            if(!previous_pressed&&effective_pressed)state->release_started.reset();
            else if(previous_pressed&&!effective_pressed){
                if(!interaction.pressed&&interaction.hovered){
                    auto interval=state->motion->duration(Policy::post_release_duration(state->authored));
                    if(!interval)return std::unexpected(interval.error());
                    state->release_started=now;state->release_window=*interval;
                }else state->release_started.reset();
            }
        }
        if(!state->live||!state->interaction_seen){
            state->interaction_seen=state->live;
            if(!state->background.reset(target)||!state->focus_opacity.reset(focus_opacity)||!state->focus_scale.reset(focus_scale))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button visual state");
        }else{
            if(target!=state->background.target()){
                state->background_duration=Policy::programmatic_duration(state->authored);state->background_curve=WidgetMotionCurve::out_cubic;
                if(input_change){
                    state->background_curve=WidgetMotionCurve::css_ease_out;
                    if(!previous_pressed&&effective_pressed)state->background_duration=Policy::press_duration(state->authored);
                    else if(previous_pressed&&!effective_pressed)state->background_duration=Policy::release_duration(state->authored);
                    else if(previous.hovered&&!interaction.hovered&&state->release_started&&
                            now>=*state->release_started&&now-*state->release_started<state->release_window)
                        state->background_duration=Policy::post_release_duration(state->authored);
                    else state->background_duration=Policy::hover_duration(state->authored);
                }
            }
            auto policy=motion_policy(state,state->background_duration,state->background_curve);if(!policy)return std::unexpected(policy.error());
            auto opacity_policy=motion_policy(state,Policy::focus_opacity_duration(state->authored),WidgetMotionCurve::out_cubic);if(!opacity_policy)return std::unexpected(opacity_policy.error());
            auto scale_policy=motion_policy(state,Policy::focus_scale_duration(state->authored),WidgetMotionCurve::out_back);if(!scale_policy)return std::unexpected(scale_policy.error());
            if(!visual.enabled)policy->duration=std::chrono::milliseconds::zero();
            const auto enabled=state->prepared->view.configuration().motion_enabled;
            if(!state->background.retarget(target,*policy,enabled,now)||
               !state->focus_opacity.retarget(focus_opacity,*opacity_policy,enabled,now)||
               !state->focus_scale.retarget(focus_scale,*scale_policy,enabled,now))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button transition");
            if(state->background.active()||state->focus_opacity.active()||state->focus_scale.active())
                if(auto started=state->root.request_frames();!started){
                    state->background.settle();state->focus_opacity.settle();state->focus_scale.settle();
                    state->release_started.reset();
                }
        }
        state->last_interaction=interaction;
        state->last_effective_pressed=effective_pressed;
        return state->root.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,ButtonInput authored,
            std::optional<ButtonTone> forced){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        try{
            auto current=state->root.action_options();if(!current)return std::unexpected(current.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Button revision exhausted");
            const auto observed_revision=state->revision;
            auto valid=prepare(authored,state->icons);if(!valid)return std::unexpected(valid.error());
            auto effective=authored;
            // Preserve host-owned toggles across unrelated reactive option changes.
            if(effective.configuration.checkable && state->authored.configuration.checkable &&
               effective.configuration.checked==state->authored.configuration.checked)
                effective.configuration.checked=current->checked;
            if(forced&&effective.configuration.tone!=ButtonTone::disabled)effective.configuration.tone=*forced;
            auto prepared=prepare(effective,state->icons);if(!prepared)return std::unexpected(prepared.error());
            if(state->revision!=observed_revision)return {};
            if(!state->options_depth)if(auto synced=synchronize_displayed(state,current->checked);!synced)return synced;
            struct OptionsGuard final {
                State& state;
                explicit OptionsGuard(State& value):state(value){++state.options_depth;}
                ~OptionsGuard(){--state.options_depth;}
            } guard(*state);
            auto previous=state->authored;auto previous_snapshot=state->prepared;auto previous_forced=state->forced_tone;
            const auto revision=++state->revision;state->authored=std::move(authored);state->prepared=std::move(*prepared);state->forced_tone=forced;
            if(auto changed=state->root.set_action_options(action_options(state->prepared));!changed){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(previous_snapshot);state->forced_tone=previous_forced;}
                return changed;
            }
            if(state->revision!=revision)return {};
            if(auto current=state->root.interaction();current)return transition(state,*current,false);
            else return std::unexpected(current.error());
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Button update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const ButtonInput& input,ii::runtime::Task action={}){
        auto state=std::make_shared<State>();
        auto prepared=prepare(input,state->icons);if(!prepared)return std::unexpected(prepared.error());
        state->authored=input;state->prepared=std::move(*prepared);
        auto motion=builder.motion_preferences();if(!motion)return std::unexpected(motion.error());state->motion=*motion;
        const auto initial=state->prepared->view.visual({input.configuration.enabled,false,false,false,input.configuration.checked});
        if(!state->background.reset(channels(initial.background)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button color");
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->prepared),[weak,action=std::move(action)]{
            auto state=weak.lock();if(!state||!Policy::method_active(state->root))return;
            const auto configuration=state->authored.configuration;
            const ButtonEvent event{state->root,"clicked",configuration.enabled,true,configuration.tone};
            if(!state->methods.invoke(event,[&]{return Policy::method_active(state->root);}))return;
            if(action)action();
            if(state->live){
                if(auto current=state->root.interaction();current)(void)transition(state,*current,false);
            }
        });
        if(!root)return std::unexpected(root.error());
        state->root=*root;
        const auto event=[weak](auto member){
            return [weak,member]{
                if(auto state=weak.lock();state&&state->live){
                    auto callback=state->events.*member;
                    if(callback)callback();
                }
            };
        };
        if(auto installed=root->set_control_events({.pressed=event(&NativeButtonEvents::pressed),
                .released=event(&NativeButtonEvents::released),.canceled=event(&NativeButtonEvents::canceled)});!installed)
            return std::unexpected(installed.error());
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            using namespace std::chrono_literals;
            auto state=weak.lock();if(!state||!state->live)return false;
            if(!state->last_frame_time||now>*state->last_frame_time)state->last_frame_time=now;
            auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
            if(preferences->revision!=state->motion_revision){
                auto policy=motion_policy(state,state->background_duration,state->background_curve);if(!policy)return std::unexpected(policy.error());
                auto opacity_policy=motion_policy(state,Policy::focus_opacity_duration(state->authored),WidgetMotionCurve::out_cubic);if(!opacity_policy)return std::unexpected(opacity_policy.error());
                auto scale_policy=motion_policy(state,Policy::focus_scale_duration(state->authored),WidgetMotionCurve::out_back);if(!scale_policy)return std::unexpected(scale_policy.error());
                const auto enabled=state->prepared->view.configuration().motion_enabled;
                if(!state->background.retarget(state->background.target(),*policy,enabled,now)||
                   !state->focus_opacity.retarget(state->focus_opacity.target(),*opacity_policy,enabled,now)||
                   !state->focus_scale.retarget(state->focus_scale.target(),*scale_policy,enabled,now))
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Button transition");
            }
            const bool background_active=state->background.advance(now);
            const bool opacity_active=state->focus_opacity.advance(now);
            const bool scale_active=state->focus_scale.advance(now);
            if(auto painted=state->root.request_repaint();!painted){if(!state->live)return false;return std::unexpected(painted.error());}
            return state->live&&(background_active||opacity_active||scale_active);
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto observed=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& value){
            if(auto state=weak.lock();state&&state->live&&value.alive){
                if(auto changed=transition(state,value,true);!changed&&state->live)throw std::runtime_error(changed.error().message);
            }
        });!observed)return std::unexpected(observed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state){return paint(state,paint_state);});!installed)
            return std::unexpected(installed.error());
        if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock()){
            state->live=true;state->interaction_seen=true;
            state->last_interaction.enabled=state->prepared->view.configuration().enabled;
        }});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!registered)return std::unexpected(registered.error());
        return BasicNativeButton(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<bool> has_methods()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        return state_->methods.has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        state_->methods.set_method(std::move(method));return {};
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        return state_->methods.set_methods(std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> set_method_registry(ButtonMethod method,std::vector<ButtonMethod> methods)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        return state_->methods.set_registry(std::move(method),std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="clicked")const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        const auto& c=state_->authored.configuration;
        return Policy::invoke_methods(state_->root,state_->methods,c.enabled,c.tone,trigger);
    }
    [[nodiscard]] ii::core::Result<void> set_events(NativeButtonEvents events)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        state_->events=std::move(events);
        return {};
    }
    // Return authored options, before a segmented parent temporarily applies
    // its borderless presentation. The returned copy can be edited safely.
    [[nodiscard]] ii::core::Result<ButtonInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->authored;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Button options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(const ButtonInput& input)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        return apply(state_,input,state_->forced_tone);
    }
    // The weak port lets a parent temporarily select its child tone while the
    // child's authored options and lifetime remain independent.
    [[nodiscard]] BorderlessTonePort weak_borderless_tone_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .apply=[weak](bool force)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Button has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
                return apply(state,state->authored,force?std::optional<ButtonTone>{ButtonTone::borderless}:std::nullopt);
            },
            .retired=[weak]{auto state=weak.lock();if(!state)return true;
                if(state->owner!=std::this_thread::get_id())return false;
                auto current=state->root.snapshot();return !current&&current.error().code==ii::core::ErrorCode::closed;}
        };
    }
};
using NativeButton=BasicNativeButton<DefaultButtonHostPolicy>;
} // namespace ii::gui
