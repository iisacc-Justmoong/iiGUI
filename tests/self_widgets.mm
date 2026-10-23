#import <AppKit/AppKit.h>
#include <iiGUI/NativeWidgets.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace ii;
namespace {
void check(bool v,const char* message) { if(!v) throw std::runtime_error(message); }
template<class T> T take(core::Result<T> r) { if(!r) throw std::runtime_error(r.error().message); return std::move(*r); }
void good(core::Result<void> r) { if(!r) { if(r.error().cause) std::rethrow_exception(r.error().cause); throw std::runtime_error(r.error().message); } }
void input(gui::WidgetMount m,gui::WidgetInputKind k,gui::Rect r) { good(m.dispatch_input({.kind=k,.x=r.x+r.width/2,.y=r.y+r.height/2})); }
NSView* surface(NSView* v) { if([v.identifier isEqualToString:@"iiGUI.widget.surface"]) return v; for(NSView* c in v.subviews) if(auto found=surface(c)) return found; return nil; }
void no_controls(NSView* view) { check(![view isKindOfClass:NSControl.class] && ![view isKindOfClass:NSStackView.class],"OS widget/layout found in self-rendered tree"); for(NSView* c in view.subviews) no_controls(c); }
void pointer() {
    gui::Host host; int calls=0;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.padding=24}));
            take(b.label(root,"label",{.text="Self-rendered"}));
            take(b.button(root,"button",{.text="Click"},[&] { ++calls; })); return {};
        }));
        auto w=take(m.find("button")); auto r=take(w.snapshot()).bounds;
        input(m,gui::WidgetInputKind::pointer_move,r); check(take(w.snapshot()).hovered,"Own hover targeting failed");
        input(m,gui::WidgetInputKind::pointer_down,r); check(take(w.snapshot()).pressed && take(w.snapshot()).focused,"Own capture/focus failed");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=0,.y=400})); check(!calls && !take(w.snapshot()).pressed,"Release outside activated button");
        input(m,gui::WidgetInputKind::pointer_down,r); input(m,gui::WidgetInputKind::pointer_up,r); check(calls==1,"Own pointer action failed");
        good(w.set_enabled(false)); input(m,gui::WidgetInputKind::pointer_down,r); input(m,gui::WidgetInputKind::pointer_up,r); check(calls==1,"Disabled control activated");
        auto invalid=m.dispatch_input({.x=std::numeric_limits<double>::quiet_NaN()}); check(!invalid,"NaN pointer accepted");
        check(!m.dispatch_input({.kind=static_cast<gui::WidgetInputKind>(999)}),"Unknown input kind accepted");
        check(!m.dispatch_input({.kind=gui::WidgetInputKind::text,.text=std::string(1,char(0xff))}),"Invalid UTF-8 input accepted");
        good(host.request_close());
    }}));
}
void keyboard() {
    gui::Host host; int clicks=0,submits=0; std::string edited;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.padding=24}));
            take(b.button(root,"button",{.text="Keyboard"},[&] { ++clicks; }));
            take(b.input_field(root,"field",{.placeholder="Write"},[&](std::string s) { edited=std::move(s); },[&] { ++submits; }));
            take(b.input_field(root,"readonly",{.text="locked",.read_only=true})); return {};
        }));
        auto button=take(m.find("button")),field=take(m.find("field"));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab})); check(take(button.snapshot()).focused,"Tab did not focus first control");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space})); check(clicks==0 && take(button.snapshot()).pressed,"Key down did not retain press state");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::space})); check(clicks==1,"Keyboard release failed");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab})); check(take(field.snapshot()).focused,"Tab did not focus field");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::composition,.text="ㅎ"})); check(take(field.snapshot()).text.empty(),"IME preedit committed prematurely");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::text,.text="한🙂"})); check(edited=="한🙂","Committed UTF-8 input failed");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::backspace})); check(edited=="한","Backspace split a Unicode scalar");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::select_all}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::text,.text="replace"})); check(edited=="replace","Selection replacement failed");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter})); check(submits==1,"Input submit failed");
        auto readonly=take(m.find("readonly")); good(readonly.focus()); good(m.dispatch_input({.kind=gui::WidgetInputKind::text,.text="bad"})); check(take(readonly.snapshot()).text=="locked","Readonly field mutated");
        good(host.request_close());
    }}));
}
void controls() {
    gui::Host host; int toggles=0; double moved=-1;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.padding=24}));
            take(b.check_box(root,"check",{.text="Check"},[&](bool) { ++toggles; }));
            take(b.radio_button(root,"a",{.text="A",.checked=true,.group="g"}));
            take(b.radio_button(root,"b",{.text="B",.group="g"}));
            take(b.toggle_switch(root,"toggle",{.text="Switch"}));
            take(b.slider(root,"slider",{.minimum=0,.maximum=100,.value=0,.step=10},[&](double n) { moved=n; }));
            take(b.progress(root,"progress",{.value=30})); return {};
        }));
        auto c=take(m.find("check")); good(c.activate()); check(take(c.snapshot()).checked && toggles==1,"Self-rendered checkbox failed");
        good(take(m.find("b")).activate()); check(!take(take(m.find("a")).snapshot()).checked && take(take(m.find("b")).snapshot()).checked,"Radio group not exclusive");
        auto t=take(m.find("toggle")); good(t.activate()); check(take(t.snapshot()).checked,"Switch did not toggle");
        auto s=take(m.find("slider")); auto r=take(s.snapshot()).bounds;
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width-9,.y=r.y+r.height/2}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+r.width-9,.y=r.y+r.height/2}));
        check(take(s.snapshot()).value==100 && moved==100,"Slider capture/range mapping failed");
        good(s.focus()); good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::left})); check(moved==90,"Slider keyboard step failed");
        auto p=take(m.find("progress")); good(p.set_value(120)); check(take(p.snapshot()).value==100,"Progress not clamped");
        check(!s.set_value(std::numeric_limits<double>::quiet_NaN()),"NaN range mutation accepted");
        auto rejected=host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void> {
            auto r=b.slider({},"overflow",{.minimum=-1e308,.maximum=1e308});
            return r?core::Result<void>{}:std::unexpected(r.error());
        });
        check(!rejected && take(s.snapshot()).value==90,"Overflowing range accepted or replaced the live tree");
        check(take(host.capture_canvas()).width>0 && take(host.graphics_info()).hardware,"Control draw list failed on GPU");
        good(host.request_close());
    }}));
}
void native_events() {
    gui::Host host; int calls=0;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));
            take(b.button(root,"button",{.text="OS input"},[&] { ++calls; }));
            take(b.input_field(root,"input",{}));
            take(b.label(root,"label",{.text="Read only"})); return {};
        }));
        NSView* view=nil; for(NSWindow* w in NSApplication.sharedApplication.windows) if(w.visible && (view=surface(w.contentView))) break;
        check(view!=nil,"No OS input surface"); no_controls(view.window.contentView);
        auto r=take(take(m.find("button")).snapshot()).bounds;
        auto point=[view convertPoint:NSMakePoint(r.x+r.width/2,r.y+r.height/2) toView:nil];
        for(auto kind:{NSEventTypeLeftMouseDown,NSEventTypeLeftMouseUp}) {
            auto e=[NSEvent mouseEventWithType:kind location:point modifierFlags:0 timestamp:0 windowNumber:view.window.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
            if(kind==NSEventTypeLeftMouseDown) [view mouseDown:e]; else [view mouseUp:e];
        }
        check(calls==1,"OS pointer did not reach C++ widget action");
        for(auto kind:{NSEventTypeKeyDown,NSEventTypeKeyUp}) {
            auto e=[NSEvent keyEventWithType:kind location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:view.window.windowNumber context:nil characters:@" " charactersIgnoringModifiers:@" " isARepeat:NO keyCode:49];
            if(kind==NSEventTypeKeyDown) [view keyDown:e]; else [view keyUp:e];
        }
        check(calls==2,"OS key did not reach C++ widget action");
        auto peers=view.accessibilityChildren; check(peers.count==3 && [peers[0] isKindOfClass:NSAccessibilityElement.class],"Virtual accessibility peers unavailable");
        check([peers[0] accessibilityPerformPress] && calls==3,"Virtual accessibility action failed");
        check(![peers[0] isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)] && ![peers[2] isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)],"Read-only virtual controls advertised value editing");
        check([peers[1] isAccessibilitySelectorAllowed:@selector(setAccessibilityValue:)],"Input did not advertise editing");
        [peers[1] setAccessibilityValue:@"AX text"];
        check(take(take(m.find("input")).snapshot()).text=="AX text","Virtual input value failed");
        check(view.accessibilityFocusedUIElement==peers[1],"Virtual focus did not identify input");
        auto e=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:view.window.windowNumber context:nil characters:@"x" charactersIgnoringModifiers:@"x" isARepeat:NO keyCode:7];
        [view keyDown:e];
        check(take(take(m.find("input")).snapshot()).text=="AX textx","Actual OS key/IME client did not edit own input");
        good(host.request_close());
    }}));
}
void cache() {
    gui::Host host;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void> { take(b.label({},"text",{.text="cached"})); return {}; }));
        auto w=take(m.find("text")); auto before=take(host.graphics_info());
        good(w.set_text("cached")); (void)take(host.capture_canvas()); auto same=take(host.graphics_info());
        check(before.scene_renders==same.scene_renders,"Unchanged widget redrew GPU scene");
        good(w.set_text("changed")); (void)take(host.capture_canvas());
        check(take(host.graphics_info()).scene_renders==before.scene_renders+1,"Changed widget did not redraw exactly once");
        good(host.request_close());
    }}));
}
void tabs() {
    gui::Host host;int clicks=0,changes=0,last=-1;
    auto icon=take(gui::rgba_image(1,1,{255,255,255,255}));
    good(host.run({.opened=[&] {
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.spacing=16,.padding=24}));
            auto bar=take(b.tab_bar(root,"tabs",{},[&](int i){++changes;last=i;}));
            take(b.tab(bar,"disabled",{.text="Disabled",.icon_name="test",.icon=icon,.enabled=false}));
            take(b.tab(bar,"overview",{.text="Overview",.icon_name="test",.icon=icon},[&]{++clicks;}));
            take(b.tab(bar,"details",{.text="Details",.icon_name="test",.badge="3",.icon=icon,.style=gui::TabStyle::surface},[&]{++clicks;}));
            take(b.button(root,"icon",{.icon=icon,.icon_name="test",.icon_mode=true},[&]{++clicks;}));
            take(b.button(root,"help",{.text="?",.accessibility_name="Help",.help=true}));return {};
        }));
        auto bar=take(mount.find("tabs")),overview=take(mount.find("overview")),details=take(mount.find("details"));
        check(take(bar.snapshot()).current_index==1 && take(overview.snapshot()).selected,"TabBar did not select first enabled tab");
        const auto r=take(overview.snapshot()).bounds,d=take(details.snapshot()).bounds;
        check(r.height==32 && std::abs(r.width-d.width)<.01,"TabBar equal widths or Tab height differs");
        good(overview.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::right}));
        check(take(details.snapshot()).focused && take(overview.snapshot()).selected && !changes,"Tab arrow changed selection instead of moving focus");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));
        check(take(details.snapshot()).selected && !take(overview.snapshot()).selected && clicks==1 && changes==1 && last==2,"Tab key activation or typed index callback failed");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::home}));check(take(overview.snapshot()).focused,"Home did not skip disabled tab");
        good(bar.set_current_index(1));check(changes==1 && take(overview.snapshot()).selected,"Programmatic index must be silent");
        check(!bar.set_current_index(0) && !bar.set_current_index(9),"Disabled or out of range index accepted");
        input(mount,gui::WidgetInputKind::pointer_down,d);input(mount,gui::WidgetInputKind::pointer_up,d);
        check(take(details.snapshot()).selected && changes==2 && clicks==2,"Own pointer selection failed");
        good(details.set_enabled(false));check(take(bar.snapshot()).current_index==1 && !take(details.snapshot()).selected,"Selection not reconciled after disabling current tab");
        good(overview.set_enabled(false));check(take(bar.snapshot()).current_index==-1,"All-disabled tab group kept a selection");
        check(take(take(mount.find("help")).snapshot()).size==gui::Size{21,21},"Help button did not use authored circular metrics");
        good(take(mount.find("icon")).activate());check(clicks==3,"Own icon button action failed");
        auto invalid=host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto tab=b.tab({},"missing",{.text="Missing",.icon=icon});return tab?core::Result<void>{}:std::unexpected(tab.error());
        });
        check(!invalid && take(bar.snapshot()).attached,"Invalid Tab replaced existing tree");
        auto before=take(host.graphics_info());auto iconWidget=take(mount.find("icon"));auto iconRect=take(iconWidget.snapshot()).bounds;
        input(mount,gui::WidgetInputKind::pointer_move,iconRect);(void)take(host.capture_canvas());
        check(take(host.graphics_info()).foreground_rasterizations==before.foreground_rasterizations,"Icon hover rerasterized unchanged text and images");
        for(NSWindow* window in NSApplication.sharedApplication.windows) if(window.visible && surface(window.contentView)) no_controls(window.contentView);
        good(host.request_close());
    }}));
}
void compound() {
    gui::Host host;std::vector<int> steps;int clicked=0;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.spacing=16,.padding=24}));
            take(b.combo_box(root,"combo",{.text="Choice"},{.clicked=[&]{++clicked;}}));
            auto row=take(b.hstack(root,"row"));
            take(b.stepper(row,"both",{},[&](int d){steps.push_back(d);}));
            take(b.stepper(row,"up",{.arrow=gui::StepperArrow::up},[&](int d){steps.push_back(d);}));
            take(b.stepper(row,"down",{.arrow=gui::StepperArrow::down},[&](int d){steps.push_back(d);}));
            take(b.stepper(row,"borderlessStep",{.tone=gui::ButtonTone::borderless}));
            take(b.stepper(row,"disabledStep",{.tone=gui::ButtonTone::disabled},[&](int){++clicked;}));
            take(b.button(root,"disabledTone",{.text="Disabled",.tone=gui::ButtonTone::disabled},[&]{++clicked;}));
            auto segments=take(b.segmented_control(root,"segments"));
            take(b.button(segments,"left",{.text="One"},[&]{++clicked;}));
            take(b.button(segments,"right",{.text="Two",.tone=gui::ButtonTone::destructive},[&]{++clicked;}));
            auto plain=take(b.segmented_control(root,"plain",{.force_borderless_tone=false}));
            take(b.button(plain,"kept",{.text="Tone",.tone=gui::ButtonTone::destructive}));return {};
        }));
        auto combo=take(m.find("combo")),both=take(m.find("both")),up=take(m.find("up")),down=take(m.find("down"));
        check(take(combo.snapshot()).size==gui::Size{97,20} && take(both.snapshot()).size==gui::Size{18,18},"Authored ComboBox/Stepper metrics differ");
        auto pixel=[&](gui::Widget w,double dx,double dy) {
            auto r=take(w.snapshot()).bounds;auto image=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;
            auto x=std::size_t((r.x+dx)*scale),y=std::size_t((r.y+dy)*scale);auto offset=(y*image.width+x)*4;
            return std::array<int,3>{image.rgba[offset],image.rgba[offset+1],image.rgba[offset+2]};
        };
        auto p=pixel(combo,3,10);check(std::abs(p[0]-40)<=2 && std::abs(p[2]-40)<=2,"ComboBox authored background did not reach GPU pixels");
        input(m,gui::WidgetInputKind::pointer_move,take(combo.snapshot()).bounds);p=pixel(combo,3,10);
        check(p[0]>=43 && p[2]>=45,"ComboBox hover background did not reach GPU pixels");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_leave}));
        p=pixel(both,3,9);check(p[2]>200 && p[0]<55,"Stepper did not render its primary background");
        auto borderlessStep=take(m.find("borderlessStep"));auto br=take(borderlessStep.snapshot()).bounds;
        input(m,gui::WidgetInputKind::pointer_down,br);p=pixel(borderlessStep,3,9);
        check(std::abs(p[0]-32)<=2 && std::abs(p[1]-60)<=2 && std::abs(p[2]-91)<=2,"Borderless Stepper pressed state did not preserve accentMuted");
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));
        auto disabledStep=take(m.find("disabledStep")),disabledTone=take(m.find("disabledTone"));
        good(disabledStep.set_enabled(true));good(disabledTone.set_enabled(true));good(disabledStep.activate());good(disabledTone.activate());
        check(!take(disabledStep.snapshot()).enabled && !take(disabledTone.snapshot()).enabled && !clicked,"Disabled tone was bypassed by enabled mutation");
        auto r=take(both.snapshot()).bounds;
        for(double y:{r.y+r.height*.25,r.y+r.height*.75}) {
            good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=y}));
            good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+r.width/2,.y=y}));
        }
        good(up.activate());good(down.activate());
        good(both.focus());good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::up}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
        check(steps==std::vector<int>({1,-1,1,-1,1,-1}),"Stepper half targeting, single direction or keyboard delivery failed");
        check(!both.step(0),"Invalid step direction accepted");good(up.step(-1));check(steps.size()==6,"Up-only stepper emitted a down step");
        good(down.set_enabled(false));good(down.activate());check(steps.size()==6,"Disabled Stepper emitted a direction");
        auto segments=take(m.find("segments")),left=take(m.find("left")),right=take(m.find("right"));
        const auto a=take(left.snapshot()),b=take(right.snapshot()),s=take(segments.snapshot());
        check(s.segment_count==2 && a.tone==gui::ButtonTone::borderless && b.tone==gui::ButtonTone::borderless,"Segment styling did not follow original container contract");
        check(std::abs(b.bounds.x-a.bounds.x-a.bounds.width-2)<.01 && std::abs(s.size.height-a.size.height-7)<.01,"Segment spacing or asymmetric padding lost");
        check(take(take(m.find("kept")).snapshot()).tone==gui::ButtonTone::destructive,"forceBorderlessTone=false ignored");
        p=pixel(segments,1,s.size.height/2);check(std::abs(p[0]-49)<=2 && std::abs(p[2]-51)<=2,"Segment border did not reach GPU pixels");
        p=pixel(segments,3,s.size.height/2);check(std::abs(p[0]-35)<=2 && std::abs(p[2]-36)<=2,"Segment fill did not preserve panel08");
        good(left.activate());good(right.activate());good(combo.activate());
        check(clicked==3 && !take(left.snapshot()).selected && !take(right.snapshot()).selected,"SegmentedControl invented a selection model");
        good(combo.set_text("Updated"));check(take(combo.snapshot()).text=="Updated","ComboBox text mutation failed");
        auto bad=host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void> {
            auto r=b.combo_box({},"bad",{.tone=gui::ButtonTone::destructive});return r?core::Result<void>{}:std::unexpected(r.error());
        });check(!bad && take(combo.snapshot()).attached,"Invalid compound control replaced valid tree");
        (void)take(host.capture_canvas());
        bool stepper_peer=false;
        for(NSWindow* w in NSApplication.sharedApplication.windows) if(w.visible && surface(w.contentView)) {
            auto view=surface(w.contentView);no_controls(w.contentView);
            for(NSAccessibilityElement* peer in view.accessibilityChildren) if([peer.accessibilityIdentifier isEqualToString:@"both"]) {
                stepper_peer=true;
                check([peer.accessibilityRole isEqualToString:NSAccessibilityIncrementorRole],"Stepper AX role differs");
                check([peer accessibilityPerformIncrement] && [peer accessibilityPerformDecrement],"Virtual Stepper AX actions missing");
                check(steps.size()==8 && steps[6]==1 && steps[7]==-1,"AX adapter bypassed own direction dispatch");
            }
        }
        check(stepper_peer,"Stepper accessibility peer missing");
        good(host.request_close());
    }}));
}
void press_events() {
    gui::Host host;std::string events;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            take(b.stepper({},"step",{},[&](int d){events+=d>0?'+':'-';},
                {.clicked=[&]{events+='c';},.pressed=[&]{events+='p';},.released=[&]{events+='r';},.canceled=[&]{events+='x';}}));return {};
        }));
        auto w=take(m.find("step"));auto r=take(w.snapshot()).bounds;
        input(m,gui::WidgetInputKind::pointer_down,r);input(m,gui::WidgetInputKind::pointer_up,r);
        check(events=="prc-","Press/release/click/step order differs");events.clear();
        input(m,gui::WidgetInputKind::pointer_down,r);
        good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=-1,.y=-1}));check(events=="pr","Outside release emitted a click or canceled an ordinary release");events.clear();
        input(m,gui::WidgetInputKind::pointer_down,r);good(w.set_enabled(false));
        input(m,gui::WidgetInputKind::pointer_up,r);check(events=="px","Disable did not cancel exactly once");events.clear();
        good(w.set_enabled(true));good(w.focus());
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));
        good(m.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::space}));
        check(events=="px","Keyboard cancel repeated presses or activated on stale release");events.clear();
        input(m,gui::WidgetInputKind::pointer_down,r);good(m.dispatch_input({.kind=gui::WidgetInputKind::focus_lost}));
        check(events=="px","Focus loss did not cancel captured control");
        // A release callback may unmount its own generation. No stale click or
        // step may follow it, and the host may continue with a replacement.
        auto next=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"next"));
            take(b.stepper(root,"reentrant",{},[&](int){events+='!';},
                {.pressed=[&]{events+='p';},.released=[&] { good(host.unmount_widgets()); },.canceled=[&]{events+='x';}}));
            take(b.button(root,"other",{.text="Other"}));return {};
        }));
        auto reentrant=take(next.find("reentrant"));auto rr=take(reentrant.snapshot()).bounds;
        events.clear();good(reentrant.focus());good(next.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));
        good(take(next.find("other")).focus());good(next.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::space}));
        check(events=="px" && !take(reentrant.snapshot()).pressed,"Moving focus did not cancel captured key press");events.clear();
        input(next,gui::WidgetInputKind::pointer_down,rr);input(next,gui::WidgetInputKind::pointer_up,rr);
        check(events=="p" && !reentrant.snapshot(),"Released callback dispatched into a retired widget tree");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv) {
    try {
        std::string_view name=argc>1?argv[1]:"pointer";
        if(name=="pointer") pointer(); else if(name=="keyboard") keyboard(); else if(name=="controls") controls(); else if(name=="native_events") native_events(); else if(name=="cache") cache(); else if(name=="tabs") tabs(); else if(name=="compound") compound(); else if(name=="press_events") press_events(); else throw std::runtime_error("Unknown self-widget test");
        std::cout<<"SELF_WIDGETS_OK "<<name<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
