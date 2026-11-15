#pragma once
#include <iiGUI/ToggleSwitchModel.h>
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
#include <utility>
namespace ii::gui {
struct DefaultToggleSwitchHostPolicy final {
    using Model=ToggleSwitchModel;using Configuration=ToggleSwitchConfiguration;using Rect=ToggleSwitchRect;using Color=WidgetColor;
};
template<class Policy>
class BasicNativeToggleSwitch final {
    using ToggleSwitch=typename Policy::Model;using ToggleSwitchConfiguration=typename Policy::Configuration;
    using ToggleSwitchRect=typename Policy::Rect;using StyleColor=typename Policy::Color;
    struct State final {
        ToggleSwitch view;
        ii::gui::Widget root;
        std::function<void(bool)> toggled;
        ii::gui::WidgetFrameHandler frame;
        ToggleSwitchConfiguration authored;
        bool last_input_checked=false,live=false,retired=false;
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeToggleSwitch(std::shared_ptr<State> state):state_(std::move(state)){}

    [[nodiscard]] static ii::gui::ActionOptions action_options(const ToggleSwitch& view){
        const auto& c=view.configuration();
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .role=ii::gui::ActionRole::toggle_switch,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.track_width+(c.text.empty()?0:c.label_gap),0,0,0},
                .minimum_width=c.track_width,.height=c.track_height,.checked=c.checked};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& paint_state){
        const auto& c=state->view.configuration();
        const auto visual=state->view.visual({paint_state.enabled,paint_state.hovered,paint_state.pressed,paint_state.focused});
        double text_width=0,text_height=0;
        if(!c.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=paint_state.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());
            text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=state->view.geometry(paint_state.size.width,paint_state.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ToggleSwitch extent");
        const auto rect=[](ToggleSwitchRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
        const auto linear=[](StyleColor value){const auto channel=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(geometry->draw_track){
            const auto color=linear(visual.track);
            drawing->panels.push_back({rect(geometry->track),color,color,float(geometry->track_radius)});
            if(visual.focused){const auto focus=linear(visual.focus);const auto stroke=(std::min)({1.5,geometry->track.width/2,geometry->track.height/2});
                if(stroke>0)drawing->panels.push_back({rect(geometry->track),focus,focus,float(geometry->track_radius),float(stroke)});}
        }
        if(geometry->draw_knob){const auto color=linear(visual.knob);drawing->panels.push_back({rect(geometry->knob),color,color,float(geometry->knob_radius)});}
        if(geometry->draw_text)drawing->texts.push_back({.bounds=rect(geometry->text),.text=c.text,.color=linear(visual.text),
            .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,
            .font_family=paint_state.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(paint_state.size.width),float(paint_state.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> schedule(const std::shared_ptr<State>& state){
        if(state->view.animating()){
            if(auto started=state->root.request_frames();!started){
                // A host without frame delivery still reaches the committed target.
                state->view.finish_animation();
                (void)state->root.set_frame_handler(state->frame,false);
            }
        }else if(auto stopped=state->root.set_frame_handler(state->frame,false);!stopped)return stopped;
        return state->root.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> apply_input(const std::shared_ptr<State>& state,
            ToggleSwitch next,std::optional<bool> changed){
        if(!state->live||state->retired)return {};
        if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"ToggleSwitch revision exhausted");
        const auto previous=state->view;const auto revision=++state->revision;
        state->view=std::move(next);
        if(changed){
            if(auto applied=state->root.set_checked(*changed);!applied){
                if(state->revision==revision)state->view=previous;
                return applied;
            }
        }
        if(!state->live||state->revision!=revision)return {};
        if(auto result=schedule(state);!result)return result;
        if(state->live&&state->revision==revision&&changed&&state->toggled){auto callback=state->toggled;callback(*changed);}
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;const auto now=std::chrono::steady_clock::now();
        if(event.kind==Kind::pointer_down){
            auto next=state->view;if(!next.begin_drag(event.x,now))return false;
            if(auto applied=apply_input(state,std::move(next),std::nullopt);!applied)return std::unexpected(applied.error());
            return true;
        }
        if(!state->view.dragging())return false;
        if(event.kind==Kind::pointer_move){
            auto next=state->view;if(!next.drag_to(event.x))return false;
            if(next.position()==state->view.position()&&next.drag_moved()==state->view.drag_moved())return true;
            if(auto applied=apply_input(state,std::move(next),std::nullopt);!applied)return std::unexpected(applied.error());
            return true;
        }
        if(event.kind==Kind::pointer_up||event.kind==Kind::focus_lost||event.kind==Kind::pointer_cancel){
            bool commit=event.kind==Kind::pointer_up;
            if(commit){auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                const auto& bounds=snapshot->bounds;
                commit=state->view.drag_moved()||(event.x>=bounds.x&&event.y>=bounds.y&&
                    event.x<=bounds.x+bounds.width&&event.y<=bounds.y+bounds.height);}
            auto next=state->view;auto changed=next.end_drag(commit,now);
            if(auto applied=apply_input(state,std::move(next),changed);!applied)return std::unexpected(applied.error());
            return true;
        }
        return false;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeToggleSwitch> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const ToggleSwitchConfiguration& configuration,std::function<void(bool)> toggled={}){
        auto state=std::make_shared<State>();
        if(!state->view.set_configuration(configuration,std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ToggleSwitch configuration");
        state->authored=configuration;state->last_input_checked=configuration.checked;state->toggled=std::move(toggled);
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto owner=weak.lock()){
            auto next=owner->view;auto changed=next.activate(std::chrono::steady_clock::now());
            if(changed){auto applied=apply_input(owner,std::move(next),changed);if(!applied)throw std::runtime_error(applied.error().message);}
        }});
        if(!root)return std::unexpected(root.error());state->root=*root;
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            auto owner=weak.lock();if(!owner||!owner->live)return false;const bool active=owner->view.advance(now);
            if(auto painted=owner->root.request_repaint();!painted)return std::unexpected(painted.error());return active;
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& value){return paint(state,value);});!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_input_handler([state](const ii::gui::WidgetInput& event){return input(state,event);});!installed)return std::unexpected(installed.error());
        if(auto registered=builder.on_publish([weak]{if(auto owner=weak.lock())owner->live=true;});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_retire([weak]{if(auto owner=weak.lock()){owner->live=false;owner->retired=true;owner->toggled={};}});!registered)return std::unexpected(registered.error());
        return BasicNativeToggleSwitch(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(ToggleSwitchConfiguration value)const{
        auto state=state_;
        if(!ToggleSwitch::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ToggleSwitch configuration");
        if(auto current=state->root.action_options();!current)return std::unexpected(current.error());
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"ToggleSwitch has retired");
        if(state->authored==value)return {};
        if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"ToggleSwitch revision exhausted");
        try{
            auto authored=value;const bool incoming=value.checked;
            if(incoming==state->last_input_checked)value.checked=state->view.checked();
            auto next=state->view;
            if(!next.set_configuration(std::move(value),std::chrono::steady_clock::now()))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ToggleSwitch configuration");
            const auto previous=state->view;const auto previous_authored=state->authored;
            const auto previous_input=state->last_input_checked;const auto revision=++state->revision;
            state->view=std::move(next);state->authored=std::move(authored);state->last_input_checked=incoming;
            if(auto changed=state->root.set_action_options(action_options(state->view));!changed){
                if(!state->retired&&state->revision==revision){state->view=previous;state->authored=previous_authored;state->last_input_checked=previous_input;}return changed;
            }
            if(state->retired||state->revision!=revision)return {};
            return schedule(state);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ToggleSwitch update failed",std::current_exception());}
    }
};
using NativeToggleSwitch=BasicNativeToggleSwitch<DefaultToggleSwitchHostPolicy>;
} // namespace ii::gui
