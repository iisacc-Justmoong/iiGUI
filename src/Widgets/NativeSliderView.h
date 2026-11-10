#pragma once
#include <iiGUI/SliderModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/WidgetMotion.h>
#include <iiGUI/Motion.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#if !defined(IIGUI_ACCESSIBILITY_VALUE_HANDLER_API)
#error "Slider requires accessible value support"
#endif
namespace ii::gui {
template<class Policy>
class BasicNativeSlider final {
    using Slider=typename Policy::Model;using SliderConfiguration=typename Policy::Configuration;
    using SliderType=typename Policy::Type;using SliderRect=typename Policy::Rect;
    using SliderEndpointWidths=typename Policy::EndpointWidths;using StyleColor=typename Policy::Color;
    struct DecorationImages final {
        std::shared_ptr<const ii::gui::RasterImage> symbol,minimum,maximum;
    };
    struct EndpointTextCache final {
        std::string minimum,maximum,font_family;
        SliderEndpointWidths widths;
    };
    struct State final {
        Slider view;
        std::shared_ptr<typename Policy::IconCache> icons=std::make_shared<typename Policy::IconCache>(),minimum_icons=std::make_shared<typename Policy::IconCache>(),maximum_icons=std::make_shared<typename Policy::IconCache>();
        DecorationImages images;
        std::optional<EndpointTextCache> endpoint_text;
        ii::gui::Widget root;
        WidgetMotionTransition<1> ring;
        std::shared_ptr<ii::gui::MotionPreferences> motion;
        ii::gui::WidgetFrameHandler frame;
        std::function<void(double)> moved;
        const std::thread::id owner=std::this_thread::get_id();
        double last_input_value=.5;
        std::uint64_t revision=0;
        std::uint64_t motion_revision=0;
        std::uint64_t request_revision=0;
        bool dragging=false,live=false,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeSlider(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action(const SliderConfiguration& value){
        ii::gui::ActionOptions options;options.accessibility_name=value.accessibility_name;
        options.enabled=value.enabled;options.focus_ring=false;options.width=value.width;options.height=value.height;return options;
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const SliderConfiguration& value){
        ii::gui::AccessibilitySemantics result;result.role=ii::gui::AccessibilityRole::slider;
        result.name=value.accessibility_name;result.range=ii::gui::AccessibilityRange{value.minimum,value.maximum,value.value};return result;
    }
    [[nodiscard]] static ii::core::Result<DecorationImages> decoration_images(
            const std::shared_ptr<State>& state,const SliderConfiguration& value){
        DecorationImages result;
        auto icons=state->icons,minimum_icons=state->minimum_icons,maximum_icons=state->maximum_icons;
        if(!icons||!minimum_icons||!maximum_icons)return ii::core::fail(ii::core::ErrorCode::closed,"Slider retired");
        if(value.show_symbol){auto image=icons->resolve(value.symbol_name,{},Policy::symbol_size);
            if(!image)return std::unexpected(image.error());result.symbol=std::move(*image);}
        if(value.show_min_max&&value.show_endpoint_icons){
            if(!value.minimum_icon_name.empty()){auto image=minimum_icons->resolve(value.minimum_icon_name,{},Policy::symbol_size);
                if(!image)return std::unexpected(image.error());result.minimum=std::move(*image);}
            if(!value.maximum_icon_name.empty()){auto image=maximum_icons->resolve(value.maximum_icon_name,{},Policy::symbol_size);
                if(!image)return std::unexpected(image.error());result.maximum=std::move(*image);}
        }
        return result;
    }
    [[nodiscard]] static ii::core::Result<SliderEndpointWidths> endpoint_widths(
            const std::shared_ptr<State>& state,const SliderConfiguration& value,std::string_view family){
        if(!value.show_min_max||!value.show_labels)return SliderEndpointWidths{};
        if(state->endpoint_text&&state->endpoint_text->minimum==value.minimum_label&&
           state->endpoint_text->maximum==value.maximum_label&&state->endpoint_text->font_family==family)
            return state->endpoint_text->widths;
        const auto measure=[&](const std::string& text)->ii::core::Result<double>{
            if(text.empty())return 0.;
            auto metrics=ii::gui::measure_text_layout({.text=text,.font_size=float(Policy::endpoint_font_size),
                .font_weight=Policy::endpoint_font_weight,.font_family=std::string(family),
                .line_height=float(Policy::endpoint_line_height)});
            if(!metrics)return std::unexpected(metrics.error());
            if(!std::isfinite(metrics->width)||metrics->width<0)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Slider endpoint measurement");
            return metrics->width;
        };
        auto minimum=measure(value.minimum_label);if(!minimum)return std::unexpected(minimum.error());
        auto maximum=measure(value.maximum_label);if(!maximum)return std::unexpected(maximum.error());
        EndpointTextCache cache{value.minimum_label,value.maximum_label,std::string(family),{*minimum,*maximum}};
        state->endpoint_text=std::move(cache);return state->endpoint_text->widths;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& paint_state){
        const auto& style=state->view.configuration();
        auto widths=endpoint_widths(state,style,paint_state.font_family);if(!widths)return std::unexpected(widths.error());
        auto layout=state->view.endpoint_layout(paint_state.size.width,paint_state.size.height,*widths);
        auto geometry=state->view.geometry(paint_state.size.width,paint_state.size.height,*widths);
        if(!layout||!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider extent");
        const auto rectangle=[](SliderRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
        const auto to_linear=[](StyleColor value){const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};};
        const auto linear=[&](StyleColor value){auto result=to_linear(value);result.alpha*=paint_state.enabled?1.f:.32f;return result;};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        const auto track=rectangle(geometry->track),fill=rectangle(geometry->fill);
        const bool filled=Policy::has_filled_track(style.type);
        const bool segmented=style.type==SliderType::segmented;
        if(track.width>0){
            auto color=linear(style.palette.track);const auto radius=(std::min)(track.width,track.height)/2;
            if(filled)drawing->backdrop=ii::gui::BackdropMaterial{.bounds=track,.radius=radius,
                .blur_radius=float(Policy::blur_radius),.tint=color,
                .source=ii::gui::BackdropSource::previous_layers,
                .inner_highlight_opacity=float(Policy::highlight_opacity*(paint_state.enabled?1.:.32)),
                .inner_highlight_offset_y=float(Policy::highlight_offset_y)};
            else drawing->panels.push_back({track,color,color,radius});
        }
        if(fill.width>0){
            const auto radius=(std::min)(fill.width,fill.height)/2;
            if(filled||!style.active){auto inactive=linear(style.palette.inactive_fill);drawing->panels.push_back({fill,inactive,inactive,radius});}
            if(style.active){auto color=linear(style.palette.fill);drawing->panels.push_back({fill,color,color,radius});}
        }
        if(style.show_ticks){
            const auto size=segmented?(std::min)(Policy::maximum_tick_diameter,
                style.track_height/Policy::tick_track_divisor):Policy::tick_size;
            const auto y=segmented?geometry->track.y+(geometry->track.height-size)/2:
                geometry->track.y+geometry->track.height+Policy::tick_gap;
            const auto color=linear(style.palette.mark);
            const auto count=segmented?style.segment_count:style.tick_count;
            const auto inset=segmented?(std::min)(style.range_inset,geometry->track.width/2):0.;
            const auto length=geometry->track.width-2*inset;
            for(std::size_t i=0;i<count;++i){
                const auto x=geometry->track.x+inset+length*double(i)/double(count-1)-size/2;
                drawing->panels.push_back({rectangle({x,y,size,size}),color,color,float(size/2)});
            }
        }
        if(geometry->center_mark.width>0){const auto color=linear(style.palette.mark);
            drawing->panels.push_back({rectangle(geometry->center_mark),color,color,float(Policy::center_radius)});}
        const auto ring_opacity=std::clamp(state->ring.value()[0],0.,1.);
        if(paint_state.enabled&&ring_opacity>0){
            // The Figma ring extends three logical pixels beyond the thumb;
            // it changes neither the slider's layout nor its hit target.
            const auto& thumb=geometry->thumb;const auto outset=filled?Policy::ring_outset:3.;
            const SliderRect ring{thumb.x-outset,thumb.y-outset,thumb.width+2*outset,thumb.height+2*outset};
            auto focus=linear(style.palette.focus);focus.alpha*=float(ring_opacity);
            const auto stroke=filled?((paint_state.pressed||state->dragging)?Policy::pressed_ring_stroke:Policy::hover_ring_stroke):2.;
            drawing->panels.push_back({rectangle(ring),focus,focus,float(ring.width/2),float((std::min)(stroke,ring.width/2))});
        }
        if(!filled||segmented||(paint_state.enabled&&ring_opacity>0)){
            auto thumb=linear(style.palette.thumb);if(filled&&!segmented)thumb.alpha*=float(ring_opacity);
            drawing->panels.push_back({rectangle(geometry->thumb),thumb,thumb,float(style.thumb_diameter/2)});
        }
        if(style.show_symbol&&state->images.symbol){
            const auto size=Policy::symbol_size;
            const auto inset=(std::min)(style.range_inset,layout->control.width/2);
            const auto center=filled?layout->control.x+(style.right_to_left?layout->control.width-inset:inset):
                geometry->thumb.x+geometry->thumb.width/2;
            const auto x=center-size/2;
            const auto y=(paint_state.size.height-size)/2;
            std::optional<ii::gui::Color> tint;
            if((style.symbol_name==Policy::symbol_icon||style.symbol_name==Policy::symbol_legacy_icon)&&
                    style.palette.symbol!=Policy::symbol_reference_color)
                tint=to_linear(style.palette.symbol);
            drawing->images.push_back({.bounds=rectangle({x,y,size,size}),.image=state->images.symbol,
                .tint=tint,.opacity=paint_state.enabled?1.f:.32f});
        }
        const auto image=[&](SliderRect bounds,const std::shared_ptr<const ii::gui::RasterImage>& asset){
            if(bounds.width>0&&asset)drawing->images.push_back({.bounds=rectangle(bounds),.image=asset,
                .opacity=paint_state.enabled?1.f:.32f});
        };
        image(layout->minimum_icon,state->images.minimum);image(layout->maximum_icon,state->images.maximum);
        const auto label=[&](SliderRect bounds,const std::string& value){
            if(bounds.width<=0)return;
            drawing->texts.push_back({.bounds=rectangle(bounds),.text=value,.color=linear(style.palette.label),
                .font_size=float(Policy::endpoint_font_size),.font_weight=Policy::endpoint_font_weight,
                .alignment=ii::gui::TextAlignment::left,.font_family=paint_state.font_family,
                .line_height=float(Policy::endpoint_line_height),.elide_right=true});
        };
        label(layout->minimum_label,style.minimum_label);label(layout->maximum_label,style.maximum_label);
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<WidgetMotion> motion_policy(const std::shared_ptr<State>& state){
        using namespace std::chrono_literals;
        auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
        auto duration=state->motion->duration(120ms);if(!duration)return std::unexpected(duration.error());
        state->motion_revision=preferences->revision;
        return WidgetMotion{.duration=std::chrono::duration_cast<std::chrono::milliseconds>(*duration),.curve=WidgetMotionCurve::out_cubic};
    }
    [[nodiscard]] static ii::core::Result<void> interaction(const std::shared_ptr<State>& state,const ii::gui::WidgetInteraction& value){
        const auto target=value.enabled&&(value.hovered||value.pressed||value.focused)?1.:0.;
        auto policy=motion_policy(state);if(!policy)return std::unexpected(policy.error());
        if(!value.enabled)policy->duration=std::chrono::milliseconds::zero();
        if(!state->ring.retarget({target},*policy,true,ii::runtime::Clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider transition");
        if(state->ring.active()){
            if(auto started=state->root.request_frames();!started)state->ring.settle();
        }
        return state->root.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> update_value(const std::shared_ptr<State>& state,Slider next){
        if(next.value()==state->view.value())return {};
        if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI Slider revision exhausted");
        const auto previous=state->view;const auto revision=++state->revision;state->view=std::move(next);
        try {
            if(auto changed=state->root.set_accessibility_semantics(semantics(state->view.configuration()));!changed){
                if(state->revision==revision)state->view=previous;return changed;
            }
            if(state->revision!=revision)return {};
            if(auto painted=state->root.request_repaint();!painted)return painted;
            if(state->revision==revision&&state->moved){auto callback=state->moved;callback(state->view.value());}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider input callback failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;using Key=ii::gui::WidgetKey;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        try {
            if(event.kind==Kind::focus_lost
#if defined(IIGUI_POINTER_CANCEL_API)
               || event.kind==Kind::pointer_cancel
#endif
            ){
                const bool was_dragging=std::exchange(state->dragging,false);
                if(was_dragging&&state->live)if(auto painted=state->root.request_repaint();!painted)return std::unexpected(painted.error());
                return false;
            }
            if(!state->view.configuration().enabled)return false;
            Slider next=state->view;bool handled=false;const bool was_dragging=state->dragging;
            if(event.kind==Kind::pointer_down||(state->dragging&&(event.kind==Kind::pointer_move||event.kind==Kind::pointer_up))){
                const auto revision=state->revision;
                // Explicit focus also propagates through an embedded content host,
                // allowing later keyboard events to reach this owned control.
                if(event.kind==Kind::pointer_down){if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());}
                if(state->revision!=revision)return true;
                auto snapshot=state->root.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                if(state->revision!=revision)return true;
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Slider pointer input requires local coordinates");
                if(event.kind==Kind::pointer_down)state->dragging=true;
                if(event.kind==Kind::pointer_up)state->dragging=false;
                auto widths=endpoint_widths(state,next.configuration(),snapshot->font_family);if(!widths)return std::unexpected(widths.error());
                handled=next.set_from_position(event.local_position->x,snapshot->bounds.width,*widths);
            }else if(event.kind==Kind::increment)handled=next.nudge(1);
            else if(event.kind==Kind::decrement)handled=next.nudge(-1);
            else if(event.kind==Kind::key_down){
                if(event.key==Key::left)handled=next.nudge(next.configuration().right_to_left?1:-1);
                else if(event.key==Key::right)handled=next.nudge(next.configuration().right_to_left?-1:1);
                else if(event.key==Key::down)handled=next.nudge(-1);
                else if(event.key==Key::up)handled=next.nudge(1);
                else if(event.key==Key::home)handled=next.set_value(next.configuration().minimum);
                else if(event.key==Key::end)handled=next.set_value(next.configuration().maximum);
            }
            if(!handled)return false;
            if(auto applied=update_value(state,std::move(next));!applied)return std::unexpected(applied.error());
            if(state->live&&state->dragging!=was_dragging)if(auto painted=state->root.request_repaint();!painted)return std::unexpected(painted.error());
            return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider input failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeSlider> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const SliderConfiguration& configuration,std::function<void(double)> moved={}){
        try {
            auto state=std::make_shared<State>();
            if(!state->view.set_configuration(configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider configuration");
            auto images=decoration_images(state,configuration);if(!images)return std::unexpected(images.error());state->images=std::move(*images);
            auto motion=builder.motion_preferences();if(!motion)return std::unexpected(motion.error());state->motion=*motion;
            state->last_input_value=configuration.value;state->moved=std::move(moved);
            auto root=builder.action_surface(parent,id,action(state->view.configuration()));if(!root)return std::unexpected(root.error());state->root=*root;
            std::weak_ptr<State> weak=state;
            state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
                auto state=weak.lock();if(!state||!state->live)return false;
                auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
                if(preferences->revision!=state->motion_revision){
                    auto policy=motion_policy(state);if(!policy)return std::unexpected(policy.error());
                    if(!state->ring.retarget(state->ring.target(),*policy,true,now))
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider transition");
                }
                const bool active=state->ring.advance(now);
                if(auto painted=state->root.request_repaint();!painted){if(!state->live)return false;return std::unexpected(painted.error());}
                return state->live&&active;
            };
            if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
            if(auto observed=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& value){
                if(auto state=weak.lock();state&&state->live&&value.alive){
                    if(auto changed=interaction(state,value);!changed&&state->live)throw std::runtime_error(changed.error().message);
                }
            });!observed)return std::unexpected(observed.error());
            if(auto accessible=root->set_accessibility_semantics(semantics(state->view.configuration()));!accessible)return std::unexpected(accessible.error());
            if(auto installed=root->set_accessibility_value_handler([state](double value)->ii::core::Result<void>{
                if(!state->live||!state->view.configuration().enabled)return ii::core::fail(ii::core::ErrorCode::closed,"LVUI Slider is not available");
                auto next=state->view;if(!next.set_value(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider value");
                return update_value(state,std::move(next));
            });!installed)return std::unexpected(installed.error());
            if(auto installed=root->set_paint_handler([state](const auto& value){return paint(state,value);});!installed)return std::unexpected(installed.error());
            if(auto installed=root->set_input_handler([state](const auto& event){return input(state,event);});!installed)return std::unexpected(installed.error());
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired)state->live=true;});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->live=false;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                if(state->request_revision!=(std::numeric_limits<std::uint64_t>::max)())++state->request_revision;
                state->moved={};state->images={};state->icons.reset();state->minimum_icons.reset();state->maximum_icons.reset();
                state->endpoint_text.reset();state->ring.settle();state->motion.reset();state->frame={};}});!registered)return std::unexpected(registered.error());
            return BasicNativeSlider(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<SliderConfiguration> options()const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        if(auto valid=state_->root.action_options();!valid)return std::unexpected(valid.error());
        try{return state_->view.configuration();}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(SliderConfiguration value)const {
        return apply_options(std::move(value),false);
    }
    [[nodiscard]] ii::core::Result<void> set_value(double position)const {
        auto value=options();if(!value)return std::unexpected(value.error());
        value->value=position;return apply_options(std::move(*value),true);
    }
private:
    [[nodiscard]] ii::core::Result<void> apply_options(SliderConfiguration value,bool force_value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Slider requires its owner thread");
        if(!Slider::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Slider configuration");
        if(state_->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Slider retired");
        try {
            if(auto valid=state_->root.action_options();!valid)return std::unexpected(valid.error());
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI Slider revision exhausted");
            if(state_->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Slider request revision exhausted");
            const auto request=++state_->request_revision;
            const auto incoming=value.value;if(!force_value&&incoming==state_->last_input_value)value.value=state_->view.value();
            auto next=state_->view;if(!next.set_configuration(std::move(value)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Slider configuration");
            auto images=decoration_images(state_,next.configuration());
            if(state_->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Slider retired");
            if(state_->request_revision!=request)return {};
            if(!images)return std::unexpected(images.error());
            const auto previous=state_->view;const auto previous_images=state_->images;const auto previous_input=state_->last_input_value;
            const auto previous_dragging=state_->dragging;const auto revision=++state_->revision;
            state_->view=std::move(next);state_->images=std::move(*images);state_->last_input_value=incoming;
            if(!state_->view.configuration().enabled)state_->dragging=false;
            if(auto changed=state_->root.set_action_options(action(state_->view.configuration()));!changed){
                if(state_->revision==revision){state_->view=previous;state_->images=previous_images;state_->last_input_value=previous_input;state_->dragging=previous_dragging;}return changed;
            }
            if(state_->revision!=revision)return {};
            if(auto accessible=state_->root.set_accessibility_semantics(semantics(state_->view.configuration()));!accessible){
                if(state_->revision==revision){state_->view=previous;state_->images=previous_images;state_->last_input_value=previous_input;state_->dragging=previous_dragging;}return accessible;
            }
            if(state_->revision!=revision)return {};
            return state_->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Slider update failed",std::current_exception());}
    }
};
// Independent defaults; an application may inject its own tokens and icon cache.
struct DefaultSliderHostPolicy final {
 using Model=SliderModel;using Configuration=SliderConfiguration;using Type=SliderType;
 using Rect=SliderRect;using EndpointWidths=SliderEndpointWidths;using Color=WidgetColor;using IconCache=ButtonIconCache;
 inline static constexpr double symbol_size=18,endpoint_font_size=13,endpoint_line_height=13;
 inline static constexpr unsigned endpoint_font_weight=500;
 inline static constexpr double blur_radius=6,highlight_opacity=.055,highlight_offset_y=-.5,ring_outset=3;
 inline static constexpr double pressed_ring_stroke=2,hover_ring_stroke=1,maximum_tick_diameter=4,tick_track_divisor=4;
 inline static constexpr double tick_size=2,tick_gap=3,center_radius=1;
 inline static constexpr std::string_view symbol_icon="symbol",symbol_legacy_icon="symbol";
 inline static constexpr Color symbol_reference_color{};
 [[nodiscard]] static bool has_filled_track(Type t)noexcept{return DefaultSliderModelPolicy::has_filled_track(t);}
};
using NativeSlider=BasicNativeSlider<DefaultSliderHostPolicy>;
} // namespace ii::gui
