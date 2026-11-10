#pragma once
#include <iiGUI/ListItemSegmentsModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/NativeSegmentedControlView.h>
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
#include <vector>
namespace ii::gui {
struct ListItemSegmentsAccent final {WidgetColor primary{},hover{},pressed{},subtle{},selected{};
 friend bool operator==(const ListItemSegmentsAccent&,const ListItemSegmentsAccent&)=default;};
struct ListItemSegmentsInput final {ListItemSegmentsConfiguration configuration;std::optional<ListItemSegmentsAccent> accent;bool visible=true;
 friend bool operator==(const ListItemSegmentsInput&,const ListItemSegmentsInput&)=default;};
struct DefaultListItemSegmentsHostPolicy final {
 using Model=ListItemSegmentsModel;using Input=ListItemSegmentsInput;
 using NativeButton=ii::gui::NativeButton;using NativeSegmentedControl=ii::gui::NativeSegmentedControl;
 [[nodiscard]] static bool valid_accent(const std::optional<ListItemSegmentsAccent>& a)noexcept{
  if(!a)return true;const auto valid=[](WidgetColor c){const auto n=[](double x){return std::isfinite(x)&&x>=0&&x<=1;};
   return n(c.red)&&n(c.green)&&n(c.blue)&&n(c.alpha);};
  return valid(a->primary)&&valid(a->hover)&&valid(a->pressed)&&valid(a->subtle)&&valid(a->selected);}
 [[nodiscard]] static SegmentedControlConfiguration group_input(const ListItemSegmentsConfiguration& c){
  SegmentedControlConfiguration v;v.horizontal_padding=c.horizontal_padding;v.vertical_padding=c.vertical_padding;
  v.spacing=c.spacing;v.enabled=c.enabled;v.force_borderless_tone=false;return v;}
 [[nodiscard]] static ButtonInput button_input(const ListItemSegmentsConfiguration& c,std::size_t i,const std::optional<ListItemSegmentsAccent>& a){
  ButtonInput in;const auto& option=c.options[i];auto& b=in.configuration;b.text=option.text;b.accessibility_name=b.text;
  b.width=c.segment_width;b.height=c.segment_height;if(a){b.palette.primary=a->primary;b.palette.primary_hover=a->hover;
   b.palette.primary_pressed=a->pressed;b.palette.normal_pressed=a->subtle;b.palette.selected=a->selected;}
  b.enabled=c.enabled&&option.enabled;b.tone=ButtonTone::borderless;b.checkable=true;b.checked=int(i)==c.selected_index;return in;}
};
// The content generation owns both the segmented surface and its buttons.
// Replacement is deferred while a child callback is active.
template<class Policy>
class BasicNativeListItemSegments final {
    using ListItemSegments=typename Policy::Model;using ListItemSegmentsInput=typename Policy::Input;
    using NativeButton=typename Policy::NativeButton;using NativeSegmentedControl=typename Policy::NativeSegmentedControl;
    struct Assembly final {
        std::optional<NativeSegmentedControl> group;
        std::vector<NativeButton> buttons;
    };
    struct State final {
        ListItemSegmentsInput input;
        ListItemSegments model;
        ii::gui::Widget root;
        std::shared_ptr<Assembly> assembly;
        std::function<void(int)> changed;
        std::function<void(ii::gui::Rect)> layout_handler;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        unsigned callbacks_active=0;
        bool live=false,retired=false,dirty=true;
    };
    explicit BasicNativeListItemSegments(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segments require their owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segments have retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    static void record(const std::shared_ptr<State>& state,ii::core::Error error)noexcept{if(state->retired)return;try{state->failure=std::move(error);}catch(...) {}}
    [[nodiscard]] static ii::core::Result<void> mark_radio(const NativeButton& button){
        auto options=button.root().action_options();if(!options)return std::unexpected(options.error());
        options->role=ii::gui::ActionRole::radio_button;return button.root().set_action_options(std::move(*options));
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state){
        auto assembly=state->assembly;if(!assembly||!assembly->group)return {};
        const auto revision=state->revision;const auto configuration=state->model.configuration();const auto accent=state->input.accent;
        if(assembly->buttons.size()!=configuration.options.size())return ii::core::fail(ii::core::ErrorCode::busy,"Segments await replacement");
        if(auto group=assembly->group->set_options(Policy::group_input(configuration));!group)return group;
        if(state->retired||state->revision!=revision)return {};
        for(std::size_t i=0;i<configuration.options.size();++i){
            auto& button=assembly->buttons[i];if(auto result=button.set_options(Policy::button_input(configuration,i,accent));!result)return result;
            if(state->retired||state->revision!=revision)return {};
            if(auto role=mark_radio(button);!role)return role;
            if(state->retired||state->revision!=revision)return {};
            if(auto checked=button.root().set_checked(int(i)==configuration.selected_index);!checked)return checked;
            if(state->retired||state->revision!=revision)return {};
        }
        return {};
    }
    struct CallbackGuard final {
        State& state;explicit CallbackGuard(State& value):state(value){++state.callbacks_active;}
        ~CallbackGuard(){--state.callbacks_active;}
    };
    static void choose(const std::shared_ptr<State>& state,int index)noexcept{
        try{
            if(!state->live||!state->input.visible||!state->model.configuration().enabled||index<0||
               std::size_t(index)>=state->model.configuration().options.size()||!state->model.configuration().options[std::size_t(index)].enabled)return;
            const auto command=state->model.configuration().options[std::size_t(index)].command;
            auto next=state->model;const auto selected=next.choose(index);
            if(!selected){
                if(state->assembly&&std::size_t(index)<state->assembly->buttons.size())
                    if(auto result=state->assembly->buttons[std::size_t(index)].root().set_checked(true);!result){record(state,result.error());return;}
                CallbackGuard guard(*state);if(state->live&&command)command->invoke();return;
            }
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("Segment revision exhausted");
            const auto previous=state->model;const auto revision=++state->revision;state->model=std::move(next);
            if(auto result=apply(state);!result){if(state->revision==revision){state->model=previous;(void)apply(state);}record(state,result.error());return;}
            if(!state->live||state->revision!=revision)return;
            CallbackGuard guard(*state);if(state->changed){auto callback=state->changed;callback(*selected);}
            if(state->live&&state->revision==revision&&command)command->invoke();
        }catch(...){try{record(state,{ii::core::ErrorCode::failed,"Segment activation failed",std::current_exception()});}catch(...) {}}
    }
    [[nodiscard]] static ii::core::Result<void> render(const std::shared_ptr<State>& state){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segments have retired");
        if(!state->dirty&&state->assembly)return {};
        if(state->callbacks_active)return ii::core::fail(ii::core::ErrorCode::busy,"Segment replacement is deferred until its callback returns");
        try{
            const auto revision=state->revision;auto assembly=std::make_shared<Assembly>();std::weak_ptr<State> weak=state;const auto configuration=state->model.configuration();
            const auto accent=state->input.accent;
            auto built=state->root.set_content([weak,assembly,configuration,accent](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto group=NativeSegmentedControl::create(builder,{},"surface",Policy::group_input(configuration));
                if(!group)return std::unexpected(group.error());assembly->group=*group;assembly->buttons.reserve(configuration.options.size());
                for(std::size_t i=0;i<configuration.options.size();++i){
                    auto button=NativeButton::create(builder,group->root(),"segment-"+std::to_string(i),Policy::button_input(configuration,i,accent),
                        [weak,i]{if(auto state=weak.lock())choose(state,int(i));});
                    if(!button)return std::unexpected(button.error());
                    if(auto role=mark_radio(*button);!role)return role;
                    if(auto handler=button->root().set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                        auto state=weak.lock();if(!state||!state->live||event.kind!=ii::gui::WidgetInputKind::key_down||
                            (event.key!=ii::gui::WidgetKey::left&&event.key!=ii::gui::WidgetKey::right))return false;
                        auto next=state->model;auto index=next.navigate(event.key==ii::gui::WidgetKey::left?-1:1);
                        if(index){choose(state,*index);if(state->live&&!state->dirty&&state->assembly&&std::size_t(*index)<state->assembly->buttons.size())
                            if(auto focused=state->assembly->buttons[std::size_t(*index)].root().focus();!focused)return std::unexpected(focused.error());}
                        return true;
                    });!handler)return handler;
                    assembly->buttons.push_back(*button);
                }
                return {};
            });
            if(!built)return built;
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segments retired during construction");
            if(state->revision!=revision)return {};
            state->assembly=std::move(assembly);state->dirty=false;return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segment construction failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListItemSegmentsInput input){
        if(auto valid_owner=check(state);!valid_owner)return valid_owner;
        if(!ListItemSegments::validate(input.configuration)||!Policy::valid_accent(input.accent))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segment configuration");
        try{
            if(input==state->input)return {};
            auto echo=state->input;echo.configuration.selected_index=input.configuration.selected_index;
            if(echo==input&&input.configuration.selected_index==state->model.configuration().selected_index){state->input=std::move(input);return {};}
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Segment revision exhausted");
            const auto previous=state->input;const auto old_model=state->model;const auto old_dirty=state->dirty;
            auto next=state->model;auto configuration=input.configuration;
            if(configuration.options==previous.configuration.options&&configuration.selected_index==previous.configuration.selected_index)
                configuration.selected_index=state->model.configuration().selected_index;
            if(!next.set_configuration(configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segment model");
            const bool replacement=configuration.options!=previous.configuration.options||configuration.segment_width!=previous.configuration.segment_width||
                configuration.segment_height!=previous.configuration.segment_height||configuration.horizontal_padding!=previous.configuration.horizontal_padding||
                configuration.vertical_padding!=previous.configuration.vertical_padding||configuration.spacing!=previous.configuration.spacing;
            const auto revision=++state->revision;state->input=std::move(input);state->model=std::move(next);state->dirty|=replacement;
            const auto rollback=[&]{if(state->revision==revision){state->input=previous;state->model=old_model;state->dirty=old_dirty;
                const auto size=old_model.intrinsic_size();(void)state->root.set_extent({size.width,size.height});
                (void)state->root.set_visible(previous.visible);if(!state->dirty)(void)apply(state);}};
            const auto size=state->model.intrinsic_size();
            if(auto result=state->root.set_extent({size.width,size.height});!result){rollback();return result;}
            if(state->retired||state->revision!=revision)return {};
            if(auto result=state->root.set_visible(state->input.visible);!result){rollback();return result;}
            if(state->retired||state->revision!=revision)return {};
            if(state->dirty&&!state->input.visible){state->failure.reset();return {};}
            if(state->dirty&&state->callbacks_active){if(auto result=state->root.set_layout_handler(state->layout_handler);!result){rollback();return result;}return {};}
            const bool rebuilding=state->dirty;
            if(auto result=render(state);!result){rollback();return result;}
            if(!rebuilding)if(auto result=apply(state);!result){rollback();return result;}
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segment update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListItemSegments> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ListItemSegmentsInput input={},std::function<void(int)> changed={}){
        if(!ListItemSegments::validate(input.configuration)||!Policy::valid_accent(input.accent))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segment configuration");
        try{
            auto state=std::make_shared<State>();state->input=std::move(input);(void)state->model.set_configuration(state->input.configuration);state->changed=std::move(changed);
            const auto size=state->model.intrinsic_size();auto root=builder.content_host(parent,id,{size.width,size.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto visible=root->set_visible(state->input.visible);!visible)return std::unexpected(visible.error());std::weak_ptr<State> weak=state;
            state->layout_handler=[weak](ii::gui::Rect){if(auto state=weak.lock();state&&state->live&&state->dirty){
                auto result=render(state);if(!result){record(state,result.error());if(result.error().code==ii::core::ErrorCode::busy){
                    (void)state->root.set_layout_handler(state->layout_handler);throw std::runtime_error(result.error().message);}}
            }};
            if(auto handler=root->set_layout_handler(state->layout_handler);!handler)return std::unexpected(handler.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock()){
                if(state->retired)return;state->live=true;if(auto armed=state->root.set_layout_handler(state->layout_handler);!armed)record(state,armed.error());
            }});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->live=false;state->retired=true;if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->assembly.reset();state->changed={};state->layout_handler={};state->failure.reset();
                state->input.configuration.options.clear();state->input.accent.reset();state->model.release();
            }});!retired)return std::unexpected(retired.error());
            return BasicNativeListItemSegments(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segment creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<ListItemSegmentsInput> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->input;
    }
    [[nodiscard]] ii::core::Result<int> selected_index()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->model.configuration().selected_index;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemSegmentsInput input)const{return update(state_,std::move(input));}
    [[nodiscard]] ii::core::Result<ii::gui::Widget> segment(std::size_t index)const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());if(auto ready=render(state_);!ready)return std::unexpected(ready.error());
        if(state_->retired||!state_->assembly)return ii::core::fail(ii::core::ErrorCode::closed,"Segments have retired");
        if(index>=state_->assembly->buttons.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Segment index is out of range");
        return state_->assembly->buttons[index].root();
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

using ListItemSegmentsView=BasicNativeListItemSegments<DefaultListItemSegmentsHostPolicy>;
} // namespace ii::gui
