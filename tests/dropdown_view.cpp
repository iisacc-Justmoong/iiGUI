#include <iiGUI/NativeDropdownButtonView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Source final:ii::gui::ButtonIconSource {
    mutable unsigned calls=0,revision_calls=0;mutable std::uint64_t version=1;unsigned mutate_at=0;bool churn=false;mutable std::function<void()> during;
    std::uint64_t revision()const noexcept override{if(++revision_calls==mutate_at||churn)++version;return version;}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,double)const override {
        ++calls;if(during){auto callback=during;during={};callback();}
        return ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255));
    }
};
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));
    DropdownInput input;input.configuration.text="Menu";input.configuration.width=100;
    input.configuration.palette.primary={.2,.4,.6,1};input.configuration.palette.text={1,1,1,1};
    std::optional<NativeDropdownButton> dropdown;unsigned clicks=0;
    auto callback_owner=std::make_shared<int>(1);std::weak_ptr<int> weak_callback=callback_owner;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        DropdownCallbacks callbacks;callbacks.clicked=[&,keep=callback_owner]{++clicks;};
        auto created=NativeDropdownButton::create(builder,{},"dropdown",input,std::move(callbacks));
        if(!created)return std::unexpected(created.error());dropdown=*created;return {};
    }));
    callback_owner.reset();auto root=dropdown->root();(void)take(tree->drawing_snapshot({300,120}));good(root.activate());
    require(clicks==1&&!weak_callback.expired(),"Dropdown action or callback ownership changed");
    auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;
    auto next=input;next.configuration.icon_mode=true;next.icon_name="icon";next.icon_source=source;
    source->during=[&]{good(dropdown->set_options(input));};good(dropdown->set_options(next));
    require(take(dropdown->options())==input,"An older icon request overrode a reentrant request");
    good(dropdown->set_options(next));const auto calls=source->calls;good(dropdown->set_options(next));
    require(source->calls==calls,"Unchanged dropdown resolved its icon again");
    ++source->version;good(dropdown->set_options(next));
    require(source->calls==calls+1,"A changed source revision was not resolved");
    next.icon_revision=1;good(dropdown->set_options(next));
    require(source->calls==calls+2,"An explicit icon revision did not refresh the source");
    auto racing=std::make_shared<Source>();racing->mutate_at=4;
    auto racing_input=input;racing_input.configuration.icon_mode=true;racing_input.icon_name="race";racing_input.icon_source=racing;
    good(dropdown->set_options(racing_input));
    require(racing->calls>=2,"A source changed during resolution without a fresh lookup");
    auto unstable=racing_input;unstable.icon_revision=1;const auto before_unstable=racing->calls;racing->churn=true;
    auto rejected=dropdown->set_options(unstable);racing->churn=false;
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::busy&&racing->calls-before_unstable<=16&&
            take(dropdown->options())==racing_input,"An unstable source was not bounded or replaced the previous dropdown");
    unstable.icon_source.reset();racing_input.icon_source.reset();racing.reset();
    (void)take(tree->drawing_snapshot({300,120}));next.icon_source.reset();source.reset();
    good(host.clear_content());require(weak_source.expired()&&weak_callback.expired()&&!dropdown->options(),
        "Retired dropdown retained its source or callbacks");
    std::optional<NativeDropdownButton> retiring;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=NativeDropdownButton::create(builder,{},"second",input);
        if(!created)return std::unexpected(created.error());retiring=*created;return {};
    }));
    auto interrupted=std::make_shared<Source>();std::weak_ptr<Source> weak_interrupted=interrupted;
    auto pending=input;pending.configuration.icon_mode=true;pending.icon_name="icon";pending.icon_source=interrupted;
    interrupted->during=[&]{good(host.clear_content());retiring.reset();};
    auto* active=&*retiring;good(active->set_options(pending));pending.icon_source.reset();interrupted.reset();
    require(weak_interrupted.expired(),"Interrupted dropdown icon lookup retained its source");tree->close();
}
