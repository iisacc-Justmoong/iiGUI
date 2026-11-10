#include <iiGUI/NativeToggleSwitchView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{160,100}));
    ToggleSwitchConfiguration input;input.text="Snap";input.motion_enabled=false;
    input.palette.on={.2,.4,.6,1};input.palette.off={.1,.1,.1,1};input.palette.knob={1,1,1,1};
    std::optional<NativeToggleSwitch> toggle;std::vector<bool> events;
    auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=NativeToggleSwitch::create(builder,{},"switch",input,[&,keep=owner](bool checked){events.push_back(checked);});
        if(!created)return std::unexpected(created.error());toggle=*created;return {};
    }));
    owner.reset();(void)take(tree->drawing_snapshot({160,100}));const auto root=toggle->root();const auto before=take(root.snapshot());
    require(before.action_role==ActionRole::toggle_switch&&!before.checked&&!weak_owner.expired(),"Toggle state or callback ownership changed");
    auto pointer=[&](WidgetInputKind kind,double x){good(tree->input({.kind=kind,.x=x,.y=before.bounds.y+11}));};
    pointer(WidgetInputKind::pointer_down,before.bounds.x+3);pointer(WidgetInputKind::pointer_move,before.bounds.x+19);
    good(toggle->set_options(input));pointer(WidgetInputKind::pointer_cancel,before.bounds.x+19);
    require(!take(root.snapshot()).checked&&events.empty(),"Canceled drag committed a toggle");
    pointer(WidgetInputKind::pointer_down,before.bounds.x+3);pointer(WidgetInputKind::pointer_move,before.bounds.x+19);
    pointer(WidgetInputKind::pointer_up,before.bounds.x+19);
    require(take(root.snapshot()).checked&&events==std::vector<bool>{true},"Toggle drag did not commit once");
    auto invalid=input;invalid.knob_size=100;require(!toggle->set_options(invalid)&&take(root.snapshot()).checked,"Invalid toggle replaced its state");
    good(host.clear_content());require(weak_owner.expired()&&!toggle->set_options(input),"Retired toggle retained a callback or accepted updates");tree->close();
}
