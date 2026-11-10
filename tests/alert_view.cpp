#include <iiGUI/NativeAlertView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct ThrowingPolicy {
    using Base=ii::gui::DefaultAlertHostPolicy;
    using Model=Base::Model;using Input=Base::Input;using Configuration=Base::Configuration;
    using Shape=Base::Shape;using Rect=Base::Rect;using Color=Base::Color;using IconCache=Base::IconCache;
    using Button=Base::Button;using ButtonConfiguration=Base::ButtonConfiguration;
    using ButtonCallbacks=Base::ButtonCallbacks;using ButtonEvent=Base::ButtonEvent;using Callbacks=Base::Callbacks;
    inline static bool enabled=false;inline static unsigned calls=0;
    static bool valid_input(const Input& input)noexcept{return Base::valid_input(input);}
    static std::uint64_t source_revision(const Input& input)noexcept{return Base::source_revision(input);}
    static auto prepare_icon(const Input& input,const Model& model,IconCache& cache){return Base::prepare_icon(input,model,cache);}
    static auto action(const Input& input,const Model& model,unsigned index){return Base::action(input,model,index);}
    static auto description_options(const Input& input,const Model& model){return Base::description_options(input,model);}
    static ii::gui::LabelOptions title_options(const Input& input,const Configuration& config){
        ++calls;if(enabled)throw std::runtime_error("style failure");return Base::title_options(input,config);
    }
};
struct Source final:ii::gui::ButtonIconSource {
    mutable unsigned calls=0,revision_calls=0;mutable std::uint64_t version=1;unsigned mutate_at=0;bool churn=false;mutable std::function<void()> during;
    std::uint64_t revision()const noexcept override{if(++revision_calls==mutate_at||churn)++version;return version;}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,double)const override{
        ++calls;if(during){auto callback=during;during={};callback();}
        return ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255));
    }
};
}
int main(){
    using namespace ii::gui;
    unsigned publications=0;detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{++publications;};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"host",{800,600}));
    AlertInput input;input.configuration.show_icon=false;input.configuration.motion_enabled=false;
    input.configuration.title="Notice";input.configuration.action_text={"Apply","Cancel",""};
    input.style.title.font_size=26;input.style.title.line_height=26;input.style.title.font_weight=700;
    input.style.description.font_size=13;input.style.description.line_height=13;input.style.description.font_weight=500;
    std::optional<AlertView> alert;std::vector<int> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        AlertCallbacks callbacks;callbacks.methods[0]=[&](const ButtonEvent&){events.push_back(1);};
        callbacks.clicked[0]=[&]{events.push_back(2);};
        auto created=AlertView::create(builder,{},"alert",input,std::move(callbacks));
        if(!created)return std::unexpected(created.error());alert=*created;return {};
    }));
    auto root=alert->root();good(root.open_at(0,0));
    auto primary=take(root.popup_content("alert.action-0"));good(primary.activate());
    require(events==std::vector<int>{1,2},"Alert callback order changed");
    (void)take(tree->drawing_snapshot({800,600}));const auto initial_height=take(root.snapshot()).bounds.height;
    auto tall=input;tall.style.title.line_height=52;good(alert->set_options(tall));
    (void)take(tree->drawing_snapshot({800,600}));
    require(take(root.snapshot()).bounds.height>=initial_height+20,"Alert layout ignored the supplied title line height");
    good(alert->set_options(input));
    const auto before=publications;good(alert->set_options(input));
    require(publications==before,"Unchanged alert input published an update");
    auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;
    auto next=input;next.configuration.show_icon=true;next.icon_name="x";next.icon_source=source;
    source->during=[&]{good(alert->set_options(input));};
    good(alert->set_options(next));
    require(take(alert->options())==input,"An older icon request overrode a reentrant request");
    good(alert->set_options(next));auto accepted=take(alert->options());
    if(!(accepted==next))throw std::runtime_error("Alert did not accept a valid icon source: show="+
        std::to_string(accepted.configuration.show_icon)+" name="+accepted.icon_name+" calls="+std::to_string(source->calls));
    const auto calls=source->calls;++source->version;good(alert->set_options(next));
    require(source->calls==calls+1,"An explicit request ignored the changed icon source revision");
    auto racing=std::make_shared<Source>();racing->mutate_at=3;
    auto racing_input=input;racing_input.configuration.show_icon=true;racing_input.icon_name="race";racing_input.icon_source=racing;
    good(alert->set_options(racing_input));
    require(racing->calls>=2,"A source revision change during preparation was recorded without resolving it");
    auto unstable=racing_input;unstable.icon_revision=1;const auto before_unstable=racing->calls;racing->churn=true;
    auto rejected=alert->set_options(unstable);racing->churn=false;
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::busy&&racing->calls-before_unstable<=16&&
            take(alert->options())==racing_input,"An unstable source was not bounded or replaced the previous alert");
    unstable.icon_source.reset();racing_input.icon_source.reset();racing.reset();
    accepted.icon_source.reset();next.icon_source.reset();source.reset();good(host.clear_content());
    require(weak_source.expired()&&!alert->options()&&!primary.snapshot(),"Retired alert retained its icon source or widget");
    auto host2=host;std::optional<AlertView> retiring;
    good(host2.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=AlertView::create(builder,{},"second-alert",input);
        if(!created)return std::unexpected(created.error());retiring=*created;return {};
    }));
    auto interrupted=std::make_shared<Source>();std::weak_ptr<Source> weak_interrupted=interrupted;
    auto pending=input;pending.configuration.show_icon=true;pending.icon_name="x";pending.icon_source=interrupted;
    interrupted->during=[&]{good(host2.clear_content());retiring.reset();};
    auto* active=&*retiring;good(active->set_options(pending));
    pending.icon_source.reset();interrupted.reset();
    require(weak_interrupted.expired(),"An interrupted icon lookup retained its source after retirement");
    std::optional<ii::gui::BasicNativeAlert<ThrowingPolicy>> throwing;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::BasicNativeAlert<ThrowingPolicy>::create(builder,{},"throwing-alert",input);
        if(!created)return std::unexpected(created.error());throwing=*created;return {};
    }));
    good(throwing->root().open_at(0,0));(void)take(tree->drawing_snapshot({800,600}));
    const auto prior_calls=ThrowingPolicy::calls;ThrowingPolicy::enabled=true;bool escaped=false;
    try{(void)take(tree->drawing_snapshot({300,600}));}catch(...){escaped=true;}
    const auto failure=take(throwing->layout_failure());
    require(!escaped&&!failure&&ThrowingPolicy::calls==prior_calls,
            "Resizing recomputed an unchanged alert style");
    auto changed=input;changed.configuration.title="Updated notice";
    auto failed=throwing->set_options(changed);
    ThrowingPolicy::enabled=false;
    require(!failed&&failed.error().code==ii::core::ErrorCode::failed&&take(throwing->options())==input,
            "Failed alert preparation changed the committed input");
    good(throwing->set_options(changed));
    require(take(throwing->options())==changed,"An alert could not recover after a preparation failure");
    good(host.clear_content());tree->close();
}
