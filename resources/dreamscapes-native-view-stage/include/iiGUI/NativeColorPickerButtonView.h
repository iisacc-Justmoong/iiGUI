#pragma once
#include <iiGUI/ColorPickerButtonModel.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
// The framework owns the color well host, state publication and drawing.
template<class Policy>
class BasicNativeColorPickerButton final {
    using ColorPickerButton=typename Policy::Model;using ColorPickerButtonInput=typename Policy::Input;
    using NativeColorPickerButtonCallbacks=typename Policy::Callbacks;using StyleColor=typename Policy::Color;
    using ColorPickerButtonRect=typename Policy::Rect;
    struct Prepared final {ColorPickerButton model;std::shared_ptr<const ii::gui::RasterImage> ring;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        ColorPickerButtonInput authored;
        Snapshot prepared;
        ii::gui::Widget root;
        NativeColorPickerButtonCallbacks callbacks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeColorPickerButton(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ColorPickerButtonInput& input){
        if(!ColorPickerButton::validate(input.configuration)||input.source_revision<0||(input.require_source&&!input.source))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ColorPickerButton configuration");
        try{
            auto configuration=input.configuration;
            if(input.source){bool stable=false;
                for(unsigned attempt=0;attempt<4;++attempt){const auto revision=input.source->revision();auto candidate=input.source->color();
                    if(input.source->revision()!=revision)continue;
                    if(!candidate)return ii::core::fail(ii::core::ErrorCode::failed,candidate.error().message);
                    if(!ColorPickerButton::valid_color(*candidate))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Color source returned an invalid color");
                    configuration.current_color=*candidate;stable=true;break;}
                if(!stable)return ii::core::fail(ii::core::ErrorCode::busy,"Color source changed repeatedly during resolution");
            }
            auto result=std::make_shared<Prepared>();if(!result->model.set_configuration(std::move(configuration)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid ColorPickerButton configuration");
            if(input.configuration.show_hue_ring){
                // Share the immutable authored raster only while a button uses it.
                static std::mutex mutex;static std::weak_ptr<const ii::gui::RasterImage> cached;
                std::lock_guard lock(mutex);result->ring=cached.lock();
                if(!result->ring){auto image=ii::gui::png_image(Policy::hue_ring());
                    if(!image)return std::unexpected(image.error());result->ring=*image;cached=result->ring;}
            }
            return Snapshot(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ColorPickerButton preparation failed",std::current_exception());}
    }
    [[nodiscard]] static std::string description(StyleColor value){
        constexpr char digits[]="0123456789ABCDEF";std::string result="#";
        for(const auto channel:{value.red,value.green,value.blue,value.alpha}){const auto byte=static_cast<unsigned>(std::lround(channel*255));
            result+=digits[byte>>4];result+=digits[byte&15];}return result;
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->model.configuration();return {.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
            .width=c.size,.height=c.size,.accessibility_description=description(c.current_color)};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor value,double opacity=1)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha*opacity)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(const Snapshot& value,const ii::gui::WidgetPaintState& input){
        try{
            auto geometry=value->model.geometry(input.size.width,input.size.height);if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid color button paint extent");
            const auto visual=value->model.visual({input.enabled,input.hovered,input.pressed,input.focused});
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
            const auto rect=[](ColorPickerButtonRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            if(geometry->surface.width>0&&geometry->surface.height>0){auto background=linear(visual.background,visual.opacity);
                if(background.alpha>0)drawing->panels.push_back({rect(geometry->surface),background,background,float(geometry->radius)});
                auto border=linear(visual.border,visual.opacity);const auto width=float((std::min)({visual.border_width,geometry->surface.width/2,geometry->surface.height/2}));
                if(border.alpha>0&&width>0)drawing->panels.push_back({rect(geometry->surface),border,border,float(geometry->radius),width});}
            if(geometry->swatch.width>0&&geometry->swatch.height>0){auto swatch=linear(visual.swatch,visual.opacity);
                if(swatch.alpha>0)drawing->panels.push_back({rect(geometry->swatch),swatch,swatch,float(geometry->swatch_radius)});}
            if(value->ring&&geometry->ring.width>0&&geometry->ring.height>0)
                drawing->images.push_back({.bounds=rect(geometry->ring),.image=value->ring,.opacity=float(visual.opacity)});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ColorPickerButton paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeColorPickerButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ColorPickerButtonInput input={},NativeColorPickerButtonCallbacks callbacks={}){
        try{
            auto prepared=prepare(input);if(!prepared)return std::unexpected(prepared.error());
            auto state=std::make_shared<State>();state->authored=std::move(input);state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);
            std::weak_ptr<State> weak=state;const auto event=[weak](auto member){return [weak,member]{if(auto value=weak.lock();value&&value->live){auto callback=value->callbacks.*member;if(callback)callback();}};};
            auto root=builder.action_surface(parent,id,action(state->prepared),event(&NativeColorPickerButtonCallbacks::clicked));
            if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_control_events({.pressed=event(&NativeColorPickerButtonCallbacks::pressed),.released=event(&NativeColorPickerButtonCallbacks::released),
                    .canceled=event(&NativeColorPickerButtonCallbacks::canceled)});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& paint_state){return paint(state->prepared,paint_state);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto value=weak.lock())value->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto value=weak.lock()){
                value->live=false;value->retired=true;if(value->revision!=(std::numeric_limits<std::uint64_t>::max)())++value->revision;
                value->callbacks={};value->authored.source.reset();value->prepared.reset();
            }});!r)return std::unexpected(r.error());
            return BasicNativeColorPickerButton(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ColorPickerButton creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<ColorPickerButtonInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ColorPickerButton requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        try{return state_->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ColorPickerButton options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(ColorPickerButtonInput input)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"ColorPickerButton requires its owner thread");
        try{
            if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"ColorPickerButton revision exhausted");
            const auto observed=state->revision;auto next=prepare(input);if(!next)return std::unexpected(next.error());
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"ColorPickerButton retired during preparation");
            if(state->revision!=observed)return {};
            auto previous=state->authored;auto old=state->prepared;const auto revision=++state->revision;
            state->authored=std::move(input);state->prepared=std::move(*next);
            if(auto updated=state->root.set_action_options(action(state->prepared));!updated){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old);}return updated;}
            if(state->revision!=revision)return {};return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"ColorPickerButton update failed",std::current_exception());}
    }
};
} // namespace ii::gui
