#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>
#include <vector>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget button;
    int presses=0,cancels=0,activations=0,cancel_notices=0;
    bool reenter_target=false,reenter_parent=false;
    good(host.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
        if(event.kind==WidgetInputKind::key_cancel&&reenter_parent){
            reenter_parent=false;good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));return true;
        }
        return false;
    }));
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Button",.width=60,.height=30},[&]{++activations;}));
        good(button.set_frame({20,20,60,30}));
        good(button.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
            if(event.kind==WidgetInputKind::key_cancel){
                ++cancel_notices;
                if(reenter_target){reenter_target=false;good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));}
                return true;
            }return false;
        }));
        return button.set_control_events({.pressed=[&]{++presses;},.canceled=[&]{++cancels;}});
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(button.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::enter}));
    check(take(button.snapshot()).pressed&&cancels==0,"Different key canceled the held key");
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    check(presses==1&&cancels==1&&cancel_notices==2&&activations==0&&!take(button.snapshot()).pressed&&take(button.snapshot()).focused,
          "Canceled key activated the button or removed keyboard focus");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    check(activations==1&&cancels==1,"Key cancellation interrupted a pointer gesture");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));reenter_target=true;
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    check(take(button.snapshot()).pressed,"Target callback's new key press was canceled by an older event");
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    check(activations==2&&cancels==2,"Reentrant target press did not activate exactly once");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));reenter_parent=true;
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    check(take(button.snapshot()).pressed,"Ancestor callback's new key press was canceled by an older event");
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    check(activations==3&&cancels==3,"Reentrant ancestor press did not activate exactly once");
    tree->close();
    auto popup_tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto popup_host=take(popup_tree->content_host({},"popup-host",{200,100}));Widget popup,popup_button;
    int popup_cancels=0,popup_activations=0;
    good(popup_host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"popup-root",{200,100}));
        popup=take(builder.popover(root,"popup",{.width=120,.height=80}));return {};
    }));
    good(popup.set_popup_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"popup-body"));
        popup_button=take(builder.action_surface(root,"popup-button",{.accessibility_name="Popup button",.width=60,.height=30},[&]{++popup_activations;}));
        return popup_button.set_control_events({.canceled=[&]{++popup_cancels;}});
    }));
    good(popup.open_at(20,10));(void)take(popup_tree->drawing_snapshot({200,100}));good(popup_button.focus());
    good(popup_tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(popup_tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::enter}));
    good(popup_tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    check(popup_cancels==1&&popup_activations==0&&take(popup.snapshot()).opened&&take(popup_button.snapshot()).focused,
          "Key cancellation failed inside a popup");
    popup_tree->close();
    auto retiring=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto retiring_host=take(retiring->content_host({},"retiring-host",{100,60}));Widget retiring_button;
    int retired_cancels=0;
    good(retiring_host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"retiring-root",{100,60}));
        retiring_button=take(builder.action_surface(root,"retiring-button",{.accessibility_name="Retire",.width=60,.height=30}));
        return retiring_button.set_control_events({.canceled=[&]{++retired_cancels;good(retiring_host.clear_content());}});
    }));
    (void)take(retiring->drawing_snapshot({100,60}));good(retiring_button.focus());
    good(retiring->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(retiring->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    check(retired_cancels==1&&!retiring_button.snapshot(),"Cancellation did not survive widget retirement");
    retiring->close();
}
