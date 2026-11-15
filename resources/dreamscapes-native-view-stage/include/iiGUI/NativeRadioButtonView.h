#pragma once
#include <iiGUI/RadioButtonModel.h>
#include <iiGUI/RadioGroup.h>
#include <iiGUI/SrgbColor.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
struct DefaultRadioButtonHostPolicy final {
    using Model=RadioButtonModel;using Configuration=RadioButtonConfiguration;using Rect=RadioButtonRect;using Color=WidgetColor;
};
template<class Policy>
class BasicNativeRadioButton final {
    using RadioButtonModel=typename Policy::Model;using RadioButtonConfiguration=typename Policy::Configuration;
    using RadioButtonRect=typename Policy::Rect;using StyleColor=typename Policy::Color;
    struct State final:RadioGroupMember,RadioGroupNavigation,std::enable_shared_from_this<State> {
        RadioButtonModel view;
        ii::gui::Widget root;
        std::shared_ptr<RadioGroups> groups;
        std::function<void(bool)> toggled;
        ii::gui::WidgetFrameHandler frame;
        RadioButtonConfiguration authored;
        bool live=false,retired=false,last_input_checked=false,host_checked=false,dirty_paint=true;
        std::uint64_t revision=0;
        [[nodiscard]] std::string_view group()const noexcept override{return view.configuration().group;}
        [[nodiscard]] bool selected()const noexcept override{return view.checked();}
        [[nodiscard]] bool participating()const noexcept override{return live;}
        [[nodiscard]] std::optional<RadioNavigationPosition> navigation_position()const override{
            if(!live||!view.configuration().enabled)return std::nullopt;
            auto interaction=root.interaction();if(!interaction||!interaction->visible||!interaction->enabled||interaction->input_blocked)return std::nullopt;
            auto placement=root.placement();if(!placement)return std::nullopt;auto domain=placement->domain.lock();if(!domain)return std::nullopt;
            return RadioNavigationPosition{interaction->bounds.x,interaction->bounds.y,std::move(domain)};
        }
        [[nodiscard]] ii::core::Result<void> choose(){
            if(!live||!view.configuration().enabled||view.checked())return {};
            if(auto current=root.action_options();!current)return std::unexpected(current.error());
            const auto current_groups=groups;
            if(!current_groups||!current_groups->select(this->shared_from_this(),true))return ii::core::fail(ii::core::ErrorCode::busy,"Radio group did not settle");
            if(live&&view.checked()&&toggled){auto callback=toggled;callback(true);}return {};
        }
        [[nodiscard]] bool navigate_to(std::string_view expected_group) override{
            if(!live||!view.configuration().enabled||group()!=expected_group)return false;
            if(auto focused=root.focus();!focused){if(!live)return false;if(focused.error().cause)std::rethrow_exception(focused.error().cause);throw std::runtime_error(focused.error().message);}
            if(!live||!view.configuration().enabled||group()!=expected_group||!navigation_position())return false;
            if(auto selected=choose();!selected){if(selected.error().cause)std::rethrow_exception(selected.error().cause);throw std::runtime_error(selected.error().message);}
            return true;
        }
        void assign(bool value)noexcept override{
            if(view.checked()==value)return;
            if(view.set_checked(value,std::chrono::steady_clock::now())){++revision;dirty_paint=true;}
        }
        void publish()noexcept override{
            if(!live)return;
            const auto next=view.checked(),previous=host_checked;const auto version=revision;
            try{
                if(next!=host_checked){
                    host_checked=next;
                    if(auto changed=root.set_checked(next);!changed){if(version==revision)host_checked=previous;return;}
                }
                if(!live||revision!=version)return;
                if(!dirty_paint)return;
                if(view.animating()){
                    if(auto started=root.request_frames();!started){view.finish_animation();(void)root.set_frame_handler(frame,false);}
                }else (void)root.set_frame_handler(frame,false);
                if(!live||revision!=version)return;
                if(root.request_repaint()&&revision==version)dirty_paint=false;
            }catch(...){}
        }
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeRadioButton(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action_options(const RadioButtonModel& view){
        const auto& c=view.configuration();const auto height=(std::max)(c.indicator_size,c.text.empty()?0.0:(c.line_height>0?c.line_height:c.font_size));
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .role=ii::gui::ActionRole::radio_button,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.indicator_size+(c.text.empty()?0:c.label_gap),0,0,0},
                .minimum_width=c.indicator_size,.height=height,.checked=c.checked};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        const auto& c=state->view.configuration();const auto visual=state->view.visual({input.enabled,input.focused});
        double text_width=0,text_height=0;
        if(!c.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=state->view.geometry(input.size.width,input.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid RadioButton extent");
        const auto rect=[](RadioButtonRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){return ii::gui::srgb_to_linear(v);};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(geometry->draw_indicator){const auto color=linear(visual.indicator);drawing->panels.push_back({rect(geometry->indicator),color,color,float(geometry->indicator_radius)});
            if(visual.focused){const auto focus=linear(visual.focus);const auto width=(std::min)(1.5,geometry->indicator.width/2);
                if(width>0)drawing->panels.push_back({rect(geometry->indicator),focus,focus,float(geometry->indicator_radius),float(width)});}}
        if(visual.draw_dot&&geometry->draw_dot){const auto dot=linear(visual.dot);drawing->panels.push_back({rect(geometry->dot),dot,dot,float(geometry->dot_radius)});}
        if(geometry->draw_text)drawing->texts.push_back({.bounds=rect(geometry->text),.text=c.text,.color=linear(visual.text),
            .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,
            .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(input.size.width),float(input.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeRadioButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,std::shared_ptr<RadioGroups> groups,const RadioButtonConfiguration& configuration,
            std::function<void(bool)> toggled={}){
        if(!groups)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"RadioButton requires a group owner");
        auto state=std::make_shared<State>();state->groups=std::move(groups);
        if(!state->view.set_configuration(configuration,std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid RadioButton configuration");
        state->authored=configuration;state->last_input_checked=state->host_checked=configuration.checked;state->toggled=std::move(toggled);
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto owner=weak.lock()){
            auto result=owner->choose();if(!result)throw std::runtime_error(result.error().message);
        }});
        if(!root)return std::unexpected(root.error());state->root=*root;
        if(auto installed=root->set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
            using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
            if(event.kind!=WidgetInputKind::key_down||event.shift||event.control||event.alt||event.meta)return false;
            const auto direction=(event.key==WidgetKey::right||event.key==WidgetKey::down)?1:
                                 (event.key==WidgetKey::left||event.key==WidgetKey::up)?-1:0;
            auto owner=weak.lock();if(!direction||!owner||!owner->live||owner->group().empty())return false;
            try{
                auto origin=owner->navigation_position();if(!origin)return false;
                const auto current_groups=owner->groups;if(!current_groups)return false;
                const auto group=std::string(owner->group());auto members=current_groups->navigation_members(group);
                struct Candidate final{std::shared_ptr<RadioGroupNavigation> member;RadioNavigationPosition position;};
                std::vector<Candidate> candidates;candidates.reserve(members.size());
                for(auto& member:members)if(auto position=member->navigation_position();position&&position->domain==origin->domain&&
                                            std::isfinite(position->x)&&std::isfinite(position->y))
                    candidates.push_back({member,std::move(*position)});
                std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
                    return a.position.y==b.position.y?a.position.x<b.position.x:a.position.y<b.position.y;
                });
                if(!owner->live)return false;
                auto current=std::find_if(candidates.begin(),candidates.end(),[&](const auto& value){return value.member.get()==static_cast<RadioGroupNavigation*>(owner.get());});
                if(current==candidates.end())return false;
                const auto count=candidates.size(),index=std::size_t(current-candidates.begin());
                if(count>1){const auto next=direction>0?(index+1)%count:(index+count-1)%count;
                    (void)candidates[next].member->navigate_to(group);}
                return true;
            }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Radio group navigation failed",std::current_exception());}
        });!installed)return std::unexpected(installed.error());
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            auto owner=weak.lock();if(!owner||!owner->live)return false;const bool active=owner->view.advance(now);
            if(auto painted=owner->root.request_repaint();!painted)return std::unexpected(painted.error());return active;
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& value){return paint(state,value);});!installed)return std::unexpected(installed.error());
        if(!state->groups->prepare(state))return ii::core::fail(ii::core::ErrorCode::full,"Radio group membership capacity exhausted");
        if(auto registered=builder.on_retire([weak]{if(auto owner=weak.lock()){
            owner->live=false;owner->retired=true;owner->toggled={};auto groups=std::move(owner->groups);
            if(groups)groups->retire(owner);
        }});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_publish([weak]{if(auto owner=weak.lock();owner&&!owner->retired){
            auto groups=owner->groups;owner->live=true;if(groups)(void)groups->refresh(owner);
        }});!registered)return std::unexpected(registered.error());
        return BasicNativeRadioButton(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(RadioButtonConfiguration value)const{
        const auto state=state_;
        if(!RadioButtonModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid RadioButton configuration");
        if(auto current=state->root.action_options();!current)return std::unexpected(current.error());
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"RadioButton has retired");
        if(state->authored==value&&!state->dirty_paint)return {};
        auto authored=value;const bool incoming=value.checked;if(incoming==state->last_input_checked)value.checked=state->view.checked();
        auto next=state->view;
        if(!next.set_configuration(std::move(value),std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid RadioButton configuration");
        state->view=std::move(next);state->authored=std::move(authored);state->last_input_checked=incoming;state->dirty_paint=true;++state->revision;
        if(state->live){const auto groups=state->groups;
            if(!groups||!groups->refresh(state))return ii::core::fail(ii::core::ErrorCode::busy,"Radio group did not settle");
        }
        // A group notification can select this member again or change its
        // options. Publish the latest retained model, not the older candidate.
        const auto revision=state->revision;
        if(auto changed=state->root.set_action_options(action_options(state->view));!changed){
            if(!state->retired&&state->revision==revision)state->dirty_paint=true;return changed;
        }
        if(state->revision!=revision)return {};
        state->host_checked=state->view.checked();state->publish();return {};
    }
};
using NativeRadioButton=BasicNativeRadioButton<DefaultRadioButtonHostPolicy>;
} // namespace ii::gui
