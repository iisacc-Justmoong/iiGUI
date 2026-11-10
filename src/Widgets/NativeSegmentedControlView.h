#pragma once
#include <iiGUI/SegmentedControlModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/SrgbColor.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
struct DefaultSegmentedControlHostPolicy final {
    using Model=SegmentedControlModel;using Configuration=SegmentedControlConfiguration;
    using Orientation=SegmentedOrientation;using Color=WidgetColor;
    using Method=ButtonMethod;using MethodRegistry=ButtonMethodRegistry;using ToneSink=BorderlessTonePort;
    [[nodiscard]] static core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::string_view trigger){
        return DefaultButtonHostPolicy::invoke_methods(root,methods,enabled,std::nullopt,trigger.empty()?"manual":trigger);
    }
};
template<class Policy>
class BasicNativeSegmentedControl final {
    using SegmentedControl=typename Policy::Model;
    using SegmentedControlConfiguration=typename Policy::Configuration;
    using SegmentedOrientation=typename Policy::Orientation;
    using StyleColor=typename Policy::Color;
    using ButtonMethod=typename Policy::Method;
    using ButtonMethodRegistry=typename Policy::MethodRegistry;
    using SegmentedToneSink=typename Policy::ToneSink;
    using SegmentedRegistrationPort=std::function<ii::core::Result<void>(SegmentedToneSink)>;
    struct State final {
        struct Registration final {std::uint64_t id;SegmentedToneSink sink;bool applied;};
        SegmentedControl model;
        ii::gui::Widget root;
        ButtonMethodRegistry methods;
        std::vector<Registration> sinks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0,next_registration=0;
        bool retired=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeSegmentedControl(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::StackOptions layout(const SegmentedControlConfiguration& c){
        ii::gui::StackOptions result;result.spacing=c.spacing;result.alignment=ii::gui::StackAlignment::center;
        result.insets=ii::gui::Insets{c.horizontal_padding,c.vertical_padding,c.horizontal_padding,c.vertical_padding};
        result.intrinsic_width=true;return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        return ii::gui::srgb_to_linear(c);
    }
    [[nodiscard]] static ii::core::Result<void> deliver(const SegmentedToneSink& sink,bool value){
        try{return sink.apply(value);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child style failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        try{
            auto geometry=state->model.geometry(input.size.width,input.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented surface extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();const auto& value=state->model.configuration();
            if(geometry->width>0&&geometry->height>0){
                const ii::gui::Rect bounds{0,0,float(geometry->width),float(geometry->height)};
                if(value.background.alpha>0){const auto ink=linear(value.background);result->panels.push_back({bounds,ink,ink,float(geometry->radius)});}
                if(value.border.alpha>0&&geometry->border_width>0){const auto ink=linear(value.border);result->panels.push_back({bounds,ink,ink,float(geometry->radius),float(geometry->border_width)});}
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented surface paint failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeSegmentedControl> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,SegmentedControlConfiguration config={}){
        try{
            auto state=std::make_shared<State>();if(!state->model.set_configuration(std::move(config)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented control configuration");
            auto root=state->model.configuration().orientation==SegmentedOrientation::vertical
                ? builder.vstack(parent,id,layout(state->model.configuration()))
                : builder.hstack(parent,id,layout(state->model.configuration()));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_enabled(state->model.configuration().enabled);!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            std::weak_ptr<State> weak=state;
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;state->methods=ButtonMethodRegistry{};state->sinks.clear();
            }});!r)return std::unexpected(r.error());
            return BasicNativeSegmentedControl(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented control creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{auto state=state_;return state->root;}
    [[nodiscard]] ii::core::Result<bool> has_methods()const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return state->methods.has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());state->methods.set_method(std::move(method));return {};
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return state->methods.set_methods(std::move(methods));
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="manual")const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        return Policy::invoke_methods(state->root,state->methods,state->model.configuration().enabled,trigger);
    }
    [[nodiscard]] SegmentedRegistrationPort weak_tone_registration_port()const{
        std::weak_ptr<State> weak=state_;
        return [weak](SegmentedToneSink sink)->ii::core::Result<void>{
            auto state=weak.lock();if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
            return BasicNativeSegmentedControl(std::move(state)).register_tone_sink(std::move(sink));
        };
    }
    [[nodiscard]] ii::core::Result<SegmentedControlConfiguration> options()const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
        return state->model.configuration();
    }
    [[nodiscard]] ii::core::Result<std::size_t> prune_retired_tone_sinks()const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
        try{
            const auto entries=state->sinks;std::vector<std::uint64_t> retired;
            for(const auto& entry:entries)if(entry.sink.retired&&entry.sink.retired())retired.push_back(entry.id);
            if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
            const auto before=state->sinks.size();
            std::erase_if(state->sinks,[&](const auto& entry){return std::ranges::find(retired,entry.id)!=retired.end();});
            return before-state->sinks.size();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child cleanup failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> register_tone_sink(std::function<ii::core::Result<void>(bool)> sink)const{
        return register_tone_sink(SegmentedToneSink{.apply=std::move(sink)});
    }
    [[nodiscard]] ii::core::Result<void> register_tone_sink(SegmentedToneSink sink)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(state->sinks.size()>=4096){auto pruned=prune_retired_tone_sinks();if(!pruned)return std::unexpected(pruned.error());}
        if(!sink.apply||state->sinks.size()>=4096||state->next_registration==(std::numeric_limits<std::uint64_t>::max)())
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented child registration");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
        try{
            for(unsigned attempt=0;attempt<8;++attempt){
                const auto revision=state->revision;auto result=deliver(sink,state->model.configuration().force_borderless_tone);
                if(!result)return result;
                if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
                if(state->revision==revision){state->sinks.push_back({++state->next_registration,std::move(sink),state->model.configuration().force_borderless_tone});return {};}
            }
            return ii::core::fail(ii::core::ErrorCode::busy,"Segmented child style did not settle");
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented child registration failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(SegmentedControlConfiguration config)const{
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Segmented control requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
        if(!SegmentedControl::validate(config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid segmented control configuration");
        try{
            if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Segmented control revision exhausted");
            const auto previous=state->model.configuration();
            if(config==previous)return {};
            if(config.orientation!=previous.orientation) return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Segmented orientation is fixed at creation");
            const auto revision=++state->revision;
            (void)state->model.set_configuration(config);
            const auto restore=[&]{if(state->revision==revision){(void)state->model.set_configuration(previous);
                (void)state->root.set_stack_options(layout(previous));if(state->revision==revision)(void)state->root.set_enabled(previous.enabled);}};
            if(auto r=state->root.set_stack_options(layout(config));!r){restore();return r;}
            if(state->revision!=revision)return {};
            if(auto r=state->root.set_enabled(config.enabled);!r){restore();return r;}
            if(state->revision!=revision)return {};
            {
                const auto sinks=state->sinks;std::vector<typename State::Registration> changed;std::vector<std::uint64_t> closed;
                const auto mark=[&](std::uint64_t id,bool applied){
                    auto entry=std::ranges::find_if(state->sinks,[&](const auto& value){return value.id==id;});
                    if(entry!=state->sinks.end())entry->applied=applied;
                };
                for(const auto& entry:sinks){
                    if(entry.applied==config.force_borderless_tone)continue;
                    auto r=deliver(entry.sink,config.force_borderless_tone);
                    if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Segmented control has retired");
                    if(state->revision!=revision)return {};
                    if(!r&&r.error().code==ii::core::ErrorCode::closed){closed.push_back(entry.id);continue;}
                    if(!r){for(auto item=changed.rbegin();item!=changed.rend()&&state->revision==revision;++item)
                            if(auto restored=deliver(item->sink,item->applied);restored&&state->revision==revision)mark(item->id,item->applied);
                        restore();return r;}
                    changed.push_back(entry);mark(entry.id,config.force_borderless_tone);
                }
                std::erase_if(state->sinks,[&](const auto& entry){return std::ranges::find(closed,entry.id)!=closed.end();});
            }
            if(state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Segmented control update failed",std::current_exception());}
    }
};
using NativeSegmentedControl=BasicNativeSegmentedControl<DefaultSegmentedControlHostPolicy>;
} // namespace ii::gui
