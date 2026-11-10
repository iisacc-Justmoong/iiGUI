#include "../src/Platform/AndroidPointer.h"
#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <stdexcept>
#include <vector>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void gesture_preserves_editor(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget editor,button;int clicked=0,canceled=0,zoom=0;
    good(host.set_content([&](WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"root",{200,100}));
        editor=take(b.input_field(root,"editor",{.text="draft",.width=80}));
        good(editor.set_frame({0,0,80,25}));
        button=take(b.action_surface(root,"action",{.accessibility_name="Action",.width=80,.height=40,.pointer_focusable=false},[&]{++clicked;}));
        good(button.set_frame({100,30,80,40}));good(button.set_control_events({.canceled=[&]{++canceled;}}));
        return root.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
            if(event.kind==WidgetInputKind::scroll&&event.control){++zoom;return true;}return false;
        });
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(editor.focus());
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    auto before=take(editor.snapshot());check(before.focused&&before.composition_text=="한","Test editor did not begin composition");
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=2;
    const auto dispatch=[&](const AndroidPointerFrame& input){auto translated=state.translate(input);for(const auto& event:translated.events)good(tree->input(event));};
    frame.action=AndroidPointerAction::down;frame.samples={{.id=1,.tool=AndroidPointerTool::touch,.x=120,.y=45}};dispatch(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;frame.samples.push_back({.id=2,.tool=AndroidPointerTool::touch,.x=150,.y=45});dispatch(frame);
    frame.action=AndroidPointerAction::move;frame.samples[0].x=110;frame.samples[1].x=160;dispatch(frame);
    frame.action=AndroidPointerAction::secondary_up;frame.index=1;dispatch(frame);
    frame.action=AndroidPointerAction::up;frame.index=0;frame.samples.resize(1);dispatch(frame);
    auto after=take(editor.snapshot());
    check(after.focused&&after.composition_text==before.composition_text,"Pinch canceled the editor focus or IME composition");
    check(clicked==0&&canceled==1&&zoom==1,"Pinch activated a pressed control or lost its zoom listener");
    tree->close();
}
void unhandled_pinch_does_not_scroll(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget scroll;
    good(host.set_content([&](WidgetBuilder& b)->core::Result<void>{
        scroll=take(b.scroll_view({},"scroll",{.width=200,.height=100,.virtual_content_height=400}));return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=20,.y=20,.scroll_y=40,.control=true,.precise_scroll=true,.device=PointerDevice::touch}));
    check(take(scroll.snapshot()).scroll_offset==0,"Unhandled pinch scrolled the list");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=20,.y=20,.scroll_y=-40,.precise_scroll=true,.device=PointerDevice::touch}));
    check(take(scroll.snapshot()).scroll_offset==40,"Ordinary two-finger pan stopped scrolling");tree->close();
}
void captured_cancel_can_reenter(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget receiver;
    int presses=0,cancels=0,releases=0;
    good(host.set_content([&](WidgetBuilder& b)->core::Result<void>{
        receiver=take(b.absolute_stack({},"receiver",{200,100}));
        return receiver.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
            if(event.kind==WidgetInputKind::pointer_down)++presses;
            if(event.kind==WidgetInputKind::pointer_cancel){++cancels;good(tree->input({.kind=WidgetInputKind::pointer_down,.x=20,.y=20,.buttons=1}));}
            if(event.kind==WidgetInputKind::pointer_up)++releases;
            return true;
        });
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=20,.y=20,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_cancel,.x=500,.y=500}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=500,.y=500}));
    check(presses==2&&cancels==1&&releases==1,"Reentrant cancellation discarded a new capture or missed the old one");
    tree->close();
}
}
int main(){
    gesture_preserves_editor();
    unhandled_pinch_does_not_scroll();
    captured_cancel_can_reenter();
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget button;
    int clicked=0,canceled=0;std::vector<WidgetInput> seen;
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Action",.width=60,.height=30},[&]{++clicked;}));
        good(button.set_frame({20,20,60,30}));
        good(button.set_control_events({.canceled=[&]{++canceled;}}));
        return button.set_input_handler([&](const WidgetInput& input)->core::Result<bool>{seen.push_back(input);return false;});
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    AndroidPointerState state;AndroidPointerFrame frame;frame.device=9;frame.scale=2;
    frame.action=AndroidPointerAction::down;frame.samples={{.id=4,.tool=AndroidPointerTool::pen,.x=60,.y=60,.pressure=.6}};
    const auto dispatch=[&](const AndroidPointerFrame& input){auto result=state.translate(input);for(const auto& event:result.events)good(tree->input(event));};
    dispatch(frame);frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back({.id=5,.tool=AndroidPointerTool::touch,.x=350,.y=160,.pressure=1});dispatch(frame);
    frame.action=AndroidPointerAction::move;frame.index=0;frame.samples[0].x=70;frame.samples[0].pressure=.7;dispatch(frame);
    frame.action=AndroidPointerAction::up;dispatch(frame);
    check(clicked==1&&canceled==0,"Secondary touch stole the pen's retained widget activation");
    check(seen.size()>=3&&seen.front().device==PointerDevice::pen&&std::abs(seen.front().pressure-.6)<.001&&
          seen.front().device_id==((std::uint64_t{9}<<32)|4),"Retained widget did not receive the Android pen identity and pressure");
    frame.samples.resize(1);frame.action=AndroidPointerAction::down;dispatch(frame);
    frame.action=AndroidPointerAction::cancel;dispatch(frame);
    check(clicked==1&&canceled==1,"Canceled Android gesture incorrectly activated a retained widget");
    frame.action=AndroidPointerAction::down;frame.samples={{.id=6,.tool=AndroidPointerTool::touch,.x=60,.y=60}};dispatch(frame);
    frame.action=AndroidPointerAction::secondary_down;frame.index=1;
    frame.samples.push_back({.id=7,.tool=AndroidPointerTool::pen,.x=60,.y=60,.pressure=.8});dispatch(frame);
    frame.action=AndroidPointerAction::secondary_up;dispatch(frame);
    check(clicked==2&&canceled==2,"Stylus takeover did not cancel touch before activating the widget");
    tree->close();
}
