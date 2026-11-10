#pragma once
#include <iiGUI/StepperModel.h>
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
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct StepperCallbacks final {
    std::function<void(int)> stepped;
    std::function<void()> clicked,pressed,released,canceled;
};
struct DefaultStepperHostPolicy final {
    using Model=StepperModel;using Configuration=StepperConfiguration;using Arrow=StepperArrow;
    using Tone=ButtonTone;using Color=WidgetColor;using Callbacks=StepperCallbacks;
    using MethodRegistry=ButtonMethodRegistry;using Method=ButtonMethod;using Event=ButtonEvent;
    [[nodiscard]] static bool method_active(const Widget& root){return DefaultButtonHostPolicy::method_active(root);}
    [[nodiscard]] static core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::optional<Tone> tone,std::string_view trigger){
        return DefaultButtonHostPolicy::invoke_methods(root,methods,enabled,tone,trigger);
    }
};
template<class Policy>
class BasicNativeStepper final {
    using Stepper=typename Policy::Model; using StepperConfiguration=typename Policy::Configuration;
    using StepperArrow=typename Policy::Arrow; using ButtonTone=typename Policy::Tone;
    using StyleColor=typename Policy::Color; using NativeStepperCallbacks=typename Policy::Callbacks;
    using ButtonMethodRegistry=typename Policy::MethodRegistry; using ButtonMethod=typename Policy::Method;
    using ButtonEvent=typename Policy::Event;
    struct State final {
        Stepper model;
        ii::gui::Widget root;
        NativeStepperCallbacks callbacks;
        ButtonMethodRegistry methods;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool tracking=false,hovered=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeStepper(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool enabled(const StepperConfiguration& value)noexcept{
        return value.enabled&&value.tone!=ButtonTone::disabled;
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const StepperConfiguration& value){
        ii::gui::ActionOptions result;result.accessibility_name=value.accessibility_name;result.enabled=enabled(value);
        result.width=value.width;result.height=value.height;result.focus_ring=false;return result;
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const StepperConfiguration& value){
        ii::gui::AccessibilitySemantics result;result.role=ii::gui::AccessibilityRole::stepper;result.name=value.accessibility_name;
        result.can_increment=value.arrow!=StepperArrow::down;result.can_decrement=value.arrow!=StepperArrow::up;return result;
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value){
        const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(linear(value.red)),float(linear(value.green)),float(linear(value.blue)),float(value.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& view){
        try{
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto width=float(view.size.width),height=float(view.size.height);
            if(width<=0||height<=0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            const auto appearance=state->model.visual({.enabled=view.enabled,.hovered=state->hovered,.pressed=state->tracking&&state->hovered,.focused=view.focused});
            const auto radius=float((std::min)({state->model.configuration().corner_radius,double(width)/2,double(height)/2}));
            if(appearance.background.alpha>0){const auto fill=color(appearance.background);drawing->panels.push_back({{0,0,width,height},fill,fill,radius});}
            if(appearance.focused){const auto focus=color(appearance.focus);drawing->panels.push_back({{0,0,width,height},focus,focus,radius,float((std::min)({1.5,double(width)/2,double(height)/2}))});}
            ii::gui::TextRun glyph{{0,0,width,height},std::string(state->model.glyph()),color(appearance.text),
                float(std::clamp(double(height),1.,13.)),400,ii::gui::TextAlignment::center,view.font_family};
            auto metrics=ii::gui::measure_text_layout(glyph);if(!metrics)return std::unexpected(metrics.error());
            glyph.bounds.y=float((std::max)(0.,(double(height)-metrics->height)/2));glyph.bounds.height=float((std::min)(double(height),metrics->height));
            if(glyph.bounds.height>0)drawing->texts.push_back(std::move(glyph));
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state,int direction){
        if(!state->model.accepts(direction))return {};
        try{
            auto alive=state->root.snapshot();if(!alive){if(alive.error().code==ii::core::ErrorCode::closed)return {};return std::unexpected(alive.error());}
            if(!alive->enabled||!alive->visible||alive->input_blocked||!state->model.accepts(direction))return {};
            const auto revision=state->revision;
            const auto& c=state->model.configuration();const ButtonEvent event{state->root,"clicked",c.enabled,true,c.tone};
            if(!state->methods.invoke(event,[&]{return Policy::method_active(state->root);}))return {};
            if(state->callbacks.clicked){auto callback=state->callbacks.clicked;callback();}
            if(state->revision!=revision)return {};
            alive=state->root.snapshot();if(!alive){if(alive.error().code==ii::core::ErrorCode::closed)return {};return std::unexpected(alive.error());}
            if(!Policy::method_active(state->root))return {};
            if(state->callbacks.stepped){auto callback=state->callbacks.stepped;callback(direction);}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper callback failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;using Key=ii::gui::WidgetKey;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        try{
            if(event.kind==Kind::focus_lost||event.kind==Kind::pointer_cancel){
                const bool canceled=std::exchange(state->tracking,false),hovered=std::exchange(state->hovered,false);
                if(canceled){if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Stepper revision exhausted");++state->revision;}
                if(canceled||hovered)if(auto r=state->root.request_repaint();!r&&r.error().code!=ii::core::ErrorCode::closed)return std::unexpected(r.error());
                if(canceled&&state->callbacks.canceled){auto callback=state->callbacks.canceled;callback();}
                return false;
            }
            if(!enabled(state->model.configuration()))return false;
            if(event.kind==Kind::pointer_leave){if(state->hovered){state->hovered=false;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());}return false;}
            if(event.kind==Kind::pointer_move&&!state->tracking){if(!state->hovered){state->hovered=true;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());}return false;}
            if(event.kind==Kind::pointer_down){
                const auto previous=state->revision;if(auto r=state->root.focus();!r)return std::unexpected(r.error());if(previous!=state->revision)return true;
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Stepper needs local pointer coordinates");
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                if(!state->model.direction_at(event.local_position->y,snapshot->bounds.height))return false;
                if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Stepper revision exhausted");
                ++state->revision;state->tracking=true;state->hovered=true;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());
                if(state->callbacks.pressed){auto callback=state->callbacks.pressed;callback();}return true;
            }
            if(event.kind==Kind::pointer_move&&state->tracking){
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Stepper needs local pointer coordinates");
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());const auto p=*event.local_position;
                const bool inside=p.x>=0&&p.x<=snapshot->bounds.width&&p.y>=0&&p.y<=snapshot->bounds.height;
                if(state->hovered!=inside){state->hovered=inside;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());}return true;
            }
            if(event.kind==Kind::pointer_up&&state->tracking){
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Stepper needs local pointer coordinates");
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                const auto p=*event.local_position;const bool inside=p.x>=0&&p.x<=snapshot->bounds.width&&p.y>=0&&p.y<=snapshot->bounds.height;
                const auto direction=inside?state->model.direction_at(p.y,snapshot->bounds.height):std::nullopt;
                if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Stepper revision exhausted");
                const auto revision=++state->revision;state->tracking=false;state->hovered=inside;
                if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());
                if(state->callbacks.released){auto callback=state->callbacks.released;callback();}
                if(direction&&state->revision==revision){if(auto r=activate(state,*direction);!r)return std::unexpected(r.error());}
                return true;
            }
            if(event.kind==Kind::increment||event.kind==Kind::decrement||(event.kind==Kind::key_down&&(event.key==Key::up||event.key==Key::down))){
                const auto direction=event.kind==Kind::increment||(event.kind==Kind::key_down&&event.key==Key::up)?1:-1;
                if(auto r=activate(state,direction);!r)return std::unexpected(r.error());return true;
            }
            return false;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper input failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeStepper> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,StepperConfiguration options={},NativeStepperCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();if(!state->model.set_configuration(std::move(options)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Stepper configuration");
            state->callbacks=std::move(callbacks);
            auto root=builder.action_surface(parent,id,action(state->model.configuration()));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_accessibility_semantics(semantics(state->model.configuration()));!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([state](const auto& value){return input(state,value);});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            std::weak_ptr<State> weak=state;
            if(auto r=builder.on_retire([weak]{if(auto current=weak.lock()){current->callbacks={};current->methods=ButtonMethodRegistry{};}});!r)return std::unexpected(r.error());
            return BasicNativeStepper(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<bool> has_methods()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());state_->methods.set_method(std::move(method));return {};
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.set_methods(std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="clicked")const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        const auto& c=state_->model.configuration();return Policy::invoke_methods(state_->root,state_->methods,c.enabled,c.tone,trigger);
    }
    [[nodiscard]] ii::core::Result<StepperConfiguration> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->model.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(StepperConfiguration value)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Stepper requires its owner thread");
        if(!Stepper::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Stepper configuration");
        try{
            if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
            if(state_->model.configuration()==value)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Stepper revision exhausted");
            auto next=state_->model;(void)next.set_configuration(std::move(value));const auto previous=state_->model;const auto revision=++state_->revision;
            state_->model=std::move(next);
            if(auto r=state_->root.set_action_options(action(state_->model.configuration()));!r){if(state_->revision==revision)state_->model=previous;return r;}
            if(state_->revision!=revision)return {};
            if(auto r=state_->root.set_accessibility_semantics(semantics(state_->model.configuration()));!r){
                if(state_->revision==revision){state_->model=previous;(void)state_->root.set_action_options(action(previous.configuration()));}return r;}
            if(state_->revision!=revision)return {};
            return state_->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Stepper update failed",std::current_exception());}
    }
};
using NativeStepper=BasicNativeStepper<DefaultStepperHostPolicy>;
} // namespace ii::gui
