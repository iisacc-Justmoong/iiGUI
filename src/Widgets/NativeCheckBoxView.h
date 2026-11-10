#pragma once
#include <iiGUI/CheckBoxModel.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
struct DefaultCheckBoxHostPolicy {
    using Model=CheckBoxModel;using Configuration=CheckBoxConfiguration;using Interaction=CheckBoxInteraction;
    using Rect=CheckBoxRect;using Color=WidgetColor;
    [[nodiscard]] static bool authored_mark(const CheckBoxVisual& value)noexcept{return value.authored_mark;}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const RasterImage>> authored_image(bool){return std::shared_ptr<const RasterImage>{};}
};
template<class Policy>
class BasicNativeCheckBox final {
    using CheckBox=typename Policy::Model;using CheckBoxConfiguration=typename Policy::Configuration;
    using CheckBoxInteraction=typename Policy::Interaction;using CheckBoxRect=typename Policy::Rect;using StyleColor=typename Policy::Color;
    struct State final {
        CheckBox view;
        ii::gui::Widget root;
        std::shared_ptr<const ii::gui::RasterImage> mark;
        std::shared_ptr<const ii::gui::RasterImage> authored_enabled,authored_disabled;
        std::shared_ptr<const ii::gui::RasterImage> focus_mask;
        ii::gui::WidgetFrameHandler frame;
        CheckBoxInteraction interaction;
        double focus_radius=-1,focus_stroke=-1;
        std::function<void(bool)> toggled;
        CheckBoxConfiguration authored;
        bool live=false,retired=false,interaction_seen=false,last_input_checked=false;
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeCheckBox(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> mask(const CheckBox& view){
        constexpr std::uint32_t size=68;
        std::vector<std::uint8_t> rgba(size*size*4);
        for(std::uint32_t y=0;y<size;++y)for(std::uint32_t x=0;x<size;++x){
            double coverage=0;
            for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx)
                coverage+=view.mark_coverage((x+(sx+.5)/4)/size,(y+(sy+.5)/4)/size);
            const auto offset=4*(y*size+x);
            rgba[offset]=rgba[offset+1]=rgba[offset+2]=255;
            rgba[offset+3]=static_cast<std::uint8_t>(std::lround(coverage*255/16));
        }
        return ii::gui::rgba_image(size,size,std::move(rgba));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> focus_mask(double radius,double stroke){
        constexpr std::uint32_t size=68;std::vector<std::uint8_t> rgba(size*size*4);
        const auto inside=[](double x,double y,double lo,double hi,double r){
            if(x<lo||y<lo||x>hi||y>hi||lo>=hi)return false;
            const auto half=(hi-lo)/2,center=(hi+lo)/2;const auto corner=std::clamp(r,0.,half);
            const auto dx=(std::max)(0.,std::abs(x-center)-(half-corner)),dy=(std::max)(0.,std::abs(y-center)-(half-corner));
            return dx*dx+dy*dy<=corner*corner;
        };
        for(std::uint32_t y=0;y<size;++y)for(std::uint32_t x=0;x<size;++x){
            unsigned coverage=0;
            for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx){
                const auto px=(x+(sx+.5)/4)/size,py=(y+(sy+.5)/4)/size;
                coverage+=inside(px,py,0,1,radius)&&!inside(px,py,stroke,1-stroke,(std::max)(0.,radius-stroke));
            }
            const auto offset=4*(y*size+x);rgba[offset]=rgba[offset+1]=rgba[offset+2]=255;
            rgba[offset+3]=static_cast<std::uint8_t>(std::lround(coverage*255./16));
        }
        return ii::gui::rgba_image(size,size,std::move(rgba));
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const CheckBox& view){
        const auto& c=view.configuration();const auto frame=c.box_size+2*c.frame_padding;
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .role=ii::gui::ActionRole::check_box,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.frame_padding+c.box_size+(c.text.empty()?0:c.label_gap),0,c.frame_padding,0},
                .minimum_width=frame,.height=frame,.checked=c.checked};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        const auto& c=state->view.configuration();const auto visual=state->view.visual({input.enabled,input.hovered,input.pressed,input.focused});
        double text_width=0,text_height=0;
        if(!c.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());
            text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=state->view.geometry(input.size.width,input.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid CheckBox extent");
        const auto rect=[](CheckBoxRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){const auto channel=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(v.red)),float(channel(v.green)),float(channel(v.blue)),float(v.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();drawing->images_before_text=true;
        if(geometry->draw_box){
            const auto bounds=rect(geometry->box);const auto fill=linear(visual.fill);
            drawing->panels.push_back({bounds,fill,fill,float(geometry->radius)});
            const auto stroke=(std::min)(visual.border_width,geometry->box.width/2);
            if(stroke>0&&visual.border.alpha>0){const auto border=linear(visual.border);drawing->panels.push_back({bounds,border,border,float(geometry->radius),float(stroke)});}
            if(visual.draw_inner_shadow){
                const auto shadow_width=(std::min)(geometry->box.width/17,geometry->box.width/2);
                if(shadow_width>0&&c.palette.inner_shadow_soft.alpha>0){const auto soft=linear(c.palette.inner_shadow_soft);
                    drawing->panels.push_back({bounds,soft,soft,float(geometry->radius),float(shadow_width)});}
                auto strong=c.palette.inner_shadow_strong;strong.alpha*=.5;
                if(strong.alpha>0){const auto top=linear(strong);auto upper=bounds;upper.height*=.45f;
                    drawing->panels.push_back({upper,top,top,float(std::min<double>(geometry->radius,upper.height/2))});}
            }
            const auto& authored_image=c.enabled&&input.enabled?state->authored_enabled:state->authored_disabled;
            const bool has_authored=Policy::authored_mark(visual)&&bool(authored_image);
            if(visual.focused&&!has_authored){const auto focus=linear(visual.focus);const auto width=(std::min)(1.5,geometry->box.width/2);
                if(width>0)drawing->panels.push_back({bounds,focus,focus,float(geometry->radius),float(width)});}
            const auto& authored=c.enabled&&input.enabled?state->authored_enabled:state->authored_disabled;
            const bool use_authored=Policy::authored_mark(visual)&&bool(authored);
            const auto& mark=use_authored?authored:state->mark;
            if(visual.draw_mark&&mark&&state->view.mark_scale()>0){
                const auto scaled=geometry->box.width*state->view.mark_scale();
                const auto x=geometry->box.x+(geometry->box.width-scaled)/2,y=geometry->box.y+(geometry->box.height-scaled)/2;
                const auto left=(std::max)(geometry->box.x,x),top=(std::max)(geometry->box.y,y);
                const auto right=(std::min)(geometry->box.x+geometry->box.width,x+scaled),bottom=(std::min)(geometry->box.y+geometry->box.height,y+scaled);
                if(right>left&&bottom>top){
                    const auto u=std::clamp((left-x)/scaled,0.,1.),v=std::clamp((top-y)/scaled,0.,1.);
                    const auto u1=std::clamp((right-x)/scaled,0.,1.),v1=std::clamp((bottom-y)/scaled,0.,1.);
                    const float uf=float(u),vf=float(v),uw=(std::min)(1.f-uf,float(u1-u)),vh=(std::min)(1.f-vf,float(v1-v));
                    if(uw>0&&vh>0)drawing->images.push_back({.bounds={float(left),float(top),float(right-left),float(bottom-top)},
                        .image=mark,.tint=use_authored?std::optional<ii::gui::Color>{}:std::optional<ii::gui::Color>{linear(visual.mark)},
                        .source={uf,vf,uw,vh}});
                }
            }
            if(visual.focused&&has_authored){
                const auto stroke=(std::min)(1.5,geometry->box.width/2);
                const auto radius_ratio=geometry->radius/geometry->box.width,stroke_ratio=stroke/geometry->box.width;
                if(stroke>0){
                    if(!state->focus_mask||state->focus_radius!=radius_ratio||state->focus_stroke!=stroke_ratio){
                        auto focus=focus_mask(radius_ratio,stroke_ratio);if(!focus)return std::unexpected(focus.error());
                        state->focus_mask=std::move(*focus);state->focus_radius=radius_ratio;state->focus_stroke=stroke_ratio;
                    }
                    drawing->images.push_back({.bounds=bounds,.image=state->focus_mask,.tint=linear(visual.focus)});
                }
            }
        }
        if(geometry->draw_text)drawing->texts.push_back({.bounds=rect(geometry->text),.text=c.text,.color=linear(visual.text),
            .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,
            .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(input.size.width),float(input.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> schedule(const std::shared_ptr<State>& state){
        if(!state->live)return {};
        const auto revision=state->revision;
        if(state->view.animating()){
            auto started=state->root.request_frames();
            if(!state->live||state->revision!=revision)return {};
            if(!started){
                // Hosts without a frame scheduler still show the committed target.
                state->view.finish_animation();(void)state->root.set_frame_handler(state->frame,false);
            }
        }else if(auto stopped=state->root.set_frame_handler(state->frame,false);!stopped){
            if(!state->live||state->revision!=revision)return {};return stopped;
        }
        if(!state->live||state->revision!=revision)return {};
        return state->root.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> interaction(const std::shared_ptr<State>& state,const ii::gui::WidgetInteraction& current){
        if(!state->live||!current.alive)return {};
        const CheckBoxInteraction input{current.enabled,current.hovered,current.pressed,current.focused};
        if(state->interaction_seen&&state->interaction==input)return {};
        auto next=state->view;
        if(!next.set_interaction(input,std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid checkbox interaction transition");
        ++state->revision;state->view=std::move(next);state->interaction=input;state->interaction_seen=true;
        return schedule(state);
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state){
        if(!state->live||state->retired)return {};
        auto next=state->view;auto changed=next.activate(std::chrono::steady_clock::now());if(!changed)return {};
        const auto previous=state->view;const auto revision=++state->revision;state->view=std::move(next);
        if(auto applied=state->root.set_checked(*changed);!applied){
            if(state->revision==revision)state->view=previous;return applied;
        }
        if(state->revision!=revision)return {};
        if(auto animated=schedule(state);!animated)return animated;
        if(state->live&&state->revision==revision&&state->toggled){auto callback=state->toggled;callback(*changed);}
        return {};
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeCheckBox> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const CheckBoxConfiguration& configuration,std::function<void(bool)> toggled={}){
        auto state=std::make_shared<State>();
        if(!state->view.set_configuration(configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid CheckBox configuration");
        auto image=mask(state->view);if(!image)return std::unexpected(image.error());state->mark=std::move(*image);
        auto enabled=Policy::authored_image(true);if(!enabled)return std::unexpected(enabled.error());state->authored_enabled=std::move(*enabled);
        auto disabled=Policy::authored_image(false);if(!disabled)return std::unexpected(disabled.error());state->authored_disabled=std::move(*disabled);
        state->authored=configuration;state->last_input_checked=configuration.checked;state->toggled=std::move(toggled);
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto owner=weak.lock()){
            auto result=activate(owner);if(!result)throw std::runtime_error(result.error().message);
        }});
        if(!root)return std::unexpected(root.error());state->root=*root;
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            auto owner=weak.lock();if(!owner||!owner->live)return false;(void)owner->view.advance(now);
            if(auto painted=owner->root.request_repaint();!painted){if(!owner->live)return false;return std::unexpected(painted.error());}
            return owner->live&&owner->view.animating();
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto observed=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& value){
            if(auto owner=weak.lock();owner&&owner->live&&value.alive){
                if(auto result=interaction(owner,value);!result&&owner->live)throw std::runtime_error(result.error().message);
            }
        });!observed)return std::unexpected(observed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& value){return paint(state,value);});!installed)
            return std::unexpected(installed.error());
        if(auto registered=builder.on_publish([weak]{if(auto owner=weak.lock()){
                owner->live=true;if(auto result=schedule(owner);!result&&owner->live)throw std::runtime_error(result.error().message);
            }});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_retire([weak]{if(auto owner=weak.lock()){
                owner->live=false;owner->retired=true;owner->toggled={};
                owner->mark.reset();owner->authored_enabled.reset();owner->authored_disabled.reset();owner->focus_mask.reset();
            }});!registered)return std::unexpected(registered.error());
        return BasicNativeCheckBox(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<CheckBoxConfiguration> options()const{
        const auto state=state_;
        if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
        try{return state->view.configuration();}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Checkbox options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(CheckBoxConfiguration value)const{
        return apply_options(std::move(value),false);
    }
    // Explicit model writes must win even when they equal the last authored
    // value and a user has since changed the local checked state.
    [[nodiscard]] ii::core::Result<void> set_checked(bool checked)const{
        auto value=options();if(!value)return std::unexpected(value.error());
        value->checked=checked;return apply_options(std::move(*value),true);
    }
private:
    [[nodiscard]] ii::core::Result<void> apply_options(CheckBoxConfiguration value,bool force_checked)const{
        const auto state=state_;
        if(!CheckBox::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid CheckBox configuration");
        if(auto current=state->root.action_options();!current)return std::unexpected(current.error());
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"CheckBox has retired");
        if(!force_checked&&state->authored==value)return {};
        auto authored=value;
        const bool incoming=value.checked;if(!force_checked&&incoming==state->last_input_checked)value.checked=state->view.checked();
        auto next=state->view;
        if(!next.set_configuration(std::move(value),std::chrono::steady_clock::now()))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid CheckBox configuration");
        auto image=state->mark;const auto& before=state->view.configuration();const auto& after=next.configuration();
        if(before.box_size!=after.box_size||before.mark_stroke_width!=after.mark_stroke_width){
            auto generated=mask(next);if(!generated)return std::unexpected(generated.error());image=std::move(*generated);
        }
        const auto previous=state->view;const auto previous_image=state->mark;const auto previous_input=state->last_input_checked;const auto previous_authored=state->authored;
        const auto revision=++state->revision;state->view=std::move(next);state->mark=std::move(image);state->last_input_checked=incoming;state->authored=std::move(authored);
        if(auto changed=state->root.set_action_options(action_options(state->view));!changed){
            if(!state->retired&&state->revision==revision){state->view=previous;state->mark=previous_image;state->last_input_checked=previous_input;state->authored=previous_authored;}return changed;
        }
        if(!state->live||state->revision!=revision)return {};
        return schedule(state);
    }
};
using NativeCheckBox=BasicNativeCheckBox<DefaultCheckBoxHostPolicy>;
} // namespace ii::gui
