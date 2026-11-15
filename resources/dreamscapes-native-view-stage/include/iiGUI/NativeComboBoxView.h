#pragma once
#include <iiGUI/ComboBoxModel.h>
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
struct ComboBoxCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
struct DefaultComboBoxHostPolicy final {
    using Model=ComboBoxModel;using Options=ComboBoxConfiguration;using Callbacks=ComboBoxCallbacks;
    using Event=ButtonEvent;using Method=ButtonMethod;using Registry=ButtonMethodRegistry;using Color=WidgetColor;using Rect=ButtonRect;
    [[nodiscard]] static bool active(const Widget& root){return DefaultButtonHostPolicy::method_active(root);}
    [[nodiscard]] static core::Result<void> invoke(const Widget& root,const Registry& methods,bool enabled,ButtonTone tone,std::string_view trigger){return DefaultButtonHostPolicy::invoke_methods(root,methods,enabled,tone,trigger);}
};
template<class Policy>
class BasicNativeComboBox final {
    using ComboBox=typename Policy::Model;using ComboBoxConfiguration=typename Policy::Options;
    using NativeComboBoxCallbacks=typename Policy::Callbacks;using ButtonEvent=typename Policy::Event;
    using ButtonMethod=typename Policy::Method;using ButtonMethodRegistry=typename Policy::Registry;
    using StyleColor=typename Policy::Color;using ButtonRect=typename Policy::Rect;
    struct State final {
        ComboBox model;
        ii::gui::Widget root;
        NativeComboBoxCallbacks callbacks;
        ButtonMethodRegistry methods;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeComboBox(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action(const ComboBoxConfiguration& c){
        ii::gui::ActionOptions result;result.text=c.text;result.accessibility_name=c.accessibility_name;
        result.enabled=c.enabled;result.focus_ring=false;result.width=c.width;result.height=c.height;
        result.font_size=c.font_size;result.line_height=c.line_height;return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        try{
            const auto geometry=state->model.geometry(input.size.width,input.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI ComboBox extent");
            const auto visual=state->model.visual({input.enabled,input.hovered,input.pressed,input.focused});
            const auto& config=state->model.configuration();auto result=std::make_shared<ii::gui::WidgetDrawing>();
            const auto rect=[](ButtonRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            if(geometry->surface.width>0&&geometry->surface.height>0){
                if(visual.background.alpha>0){const auto color=linear(visual.background);result->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius)});}
                if(visual.focused){const auto color=linear(visual.focus);const auto stroke=float((std::min)({1.5,input.size.width/2,input.size.height/2}));
                    if(stroke>0)result->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius),stroke});}
            }
            const ii::gui::Rect clip{0,0,float(input.size.width),float(input.size.height)};
            if(geometry->label.width>0&&geometry->label.height>0&&!config.text.empty())
                result->texts.push_back({.bounds=rect(geometry->label),.text=config.text,.color=linear(visual.text),
                    .font_size=float(config.font_size),.font_weight=config.font_weight,.alignment=ii::gui::TextAlignment::left,
                    .font_family=input.font_family,.line_height=float(config.line_height),.elide_right=true,.clip=clip});
            if(geometry->indicator.width>0&&geometry->indicator.height>0){
                auto bounds=rect(geometry->indicator);
                ii::gui::TextRun glyph{bounds,std::string(state->model.glyph()),linear(visual.text),
                    float((std::min)(13.,geometry->indicator.height)),400,ii::gui::TextAlignment::center,input.font_family};
                auto metrics=ii::gui::measure_text_layout(glyph);if(!metrics)return std::unexpected(metrics.error());
                glyph.bounds.y+=float((std::max)(0.,(geometry->indicator.height-metrics->height)/2));
                glyph.bounds.height=float((std::min)(geometry->indicator.height,metrics->height));glyph.clip=clip;
                if(glyph.bounds.height>0)result->texts.push_back(std::move(glyph));
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI ComboBox paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeComboBox> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ComboBoxConfiguration config={},NativeComboBoxCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();if(!state->model.set_configuration(std::move(config)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI ComboBox configuration");
            state->callbacks=std::move(callbacks);std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member){return [weak,member]{if(auto s=weak.lock();s&&s->live){auto cb=s->callbacks.*member;if(cb)cb();}};};
            auto root=builder.action_surface(parent,id,action(state->model.configuration()),[weak]{
                auto state=weak.lock();if(!state||!state->live||!Policy::active(state->root))return;
                const auto& c=state->model.configuration();const ButtonEvent value{state->root,"clicked",c.enabled,true,c.tone};
                if(!state->methods.invoke(value,[&]{return state->live&&Policy::active(state->root);}))return;
                auto callback=state->callbacks.clicked;if(callback)callback();
            });
            if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_control_events({.pressed=event(&NativeComboBoxCallbacks::pressed),
                    .released=event(&NativeComboBoxCallbacks::released),.canceled=event(&NativeComboBoxCallbacks::canceled)});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& input){return paint(state,input);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto s=weak.lock())s->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto s=weak.lock()){s->live=false;s->retired=true;s->callbacks={};s->methods={};}});!r)return std::unexpected(r.error());
            return BasicNativeComboBox(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI ComboBox construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<bool> has_methods()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());state_->methods.set_method(std::move(method));return {};
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());return state_->methods.set_methods(std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="clicked")const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        const auto& c=state->model.configuration();return Policy::invoke(state->root,state->methods,c.enabled,c.tone,trigger);
    }
    [[nodiscard]] ii::core::Result<ComboBoxConfiguration> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->model.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ComboBox options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(ComboBoxConfiguration config)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ComboBox requires its owner thread");
        if(!ComboBox::validate(config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI ComboBox configuration");
        try{
            if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"ComboBox revision exhausted");
            auto next=state->model;(void)next.set_configuration(std::move(config));auto previous=state->model;
            const auto revision=++state->revision;state->model=std::move(next);
            if(auto r=state->root.set_action_options(action(state->model.configuration()));!r){
                if(state->revision==revision)state->model=std::move(previous);return r;
            }
            if(state->retired||state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ComboBox update failed",std::current_exception());}
    }
};
using NativeComboBox=BasicNativeComboBox<DefaultComboBoxHostPolicy>;
} // namespace ii::gui
