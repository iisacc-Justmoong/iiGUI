#include <iiGUI/NativeRadioButtonView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct RetiringPeer final:ii::gui::RadioGroupMember {
    bool active=true,checked=false;std::function<void()> on_publish;
    std::string_view group()const noexcept override{return "choice";}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{if(on_publish){auto callback=std::move(on_publish);callback();}}
};
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));auto groups=std::make_shared<RadioGroups>();
    std::weak_ptr<RadioGroups> weak_groups=groups;std::optional<NativeRadioButton> first,second;std::vector<bool> events;
    auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;
    RadioButtonConfiguration a;a.text="First";a.group="choice";a.checked=true;a.motion_enabled=false;
    RadioButtonConfiguration b=a;b.text="Second";b.checked=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto row=builder.vstack({},"row");if(!row)return std::unexpected(row.error());
        auto one=NativeRadioButton::create(builder,*row,"one",groups,a);if(!one)return std::unexpected(one.error());first=*one;
        auto two=NativeRadioButton::create(builder,*row,"two",groups,b,[&,keep=owner](bool v){events.push_back(v);});
        if(!two)return std::unexpected(two.error());second=*two;return {};
    }));
    owner.reset();(void)take(tree->drawing_snapshot({200,100}));
    require(take(first->root().snapshot()).checked&&!take(second->root().snapshot()).checked,"initial group state");
    good(second->root().activate());
    require(!take(first->root().snapshot()).checked&&take(second->root().snapshot()).checked&&events==std::vector<bool>{true},"exclusive selection");
    good(second->set_options(b));require(take(second->root().snapshot()).checked&&events.size()==1,"unchanged source reset selection");
    auto invalid=b;invalid.indicator_size=0;require(!second->set_options(invalid)&&take(second->root().snapshot()).checked,"invalid update committed");
    groups.reset();good(host.clear_content());
    require(weak_owner.expired()&&weak_groups.expired()&&!second->set_options(b),"retired radio retained resources or accepted updates");
    tree->close();

    auto second_tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto second_host=take(second_tree->content_host({},"host",{100,50}));auto transient_groups=std::make_shared<RadioGroups>();
    auto peer=std::make_shared<RetiringPeer>();require(transient_groups->prepare(peer),"peer registration");
    std::weak_ptr<RadioGroups> transient_weak=transient_groups;std::optional<NativeRadioButton> transient;
    good(second_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=NativeRadioButton::create(builder,{},"radio",transient_groups,b);
        if(!result)return std::unexpected(result.error());transient=*result;return {};
    }));
    bool fired=false,alive_during_callback=false;peer->on_publish=[&]{
        fired=true;(void)second_host.clear_content();alive_during_callback=!transient_weak.expired();
    };
    transient_groups.reset();(void)transient->root().activate();
    require(fired&&alive_during_callback&&transient_weak.expired()&&!transient->root().snapshot(),
            "group was not retained safely across reentrant retirement");
    second_tree->close();
}
