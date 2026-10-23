#import <AppKit/AppKit.h>
#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
NSView* surface(NSView* v){if([v.identifier isEqualToString:@"iiGUI.widget.surface"])return v;for(NSView* c in v.subviews)if(auto s=surface(c))return s;return nil;}
void no_controls(NSView* v){check(![v isKindOfClass:NSControl.class],"OS color control was substituted");for(NSView* child in v.subviews)no_controls(child);}
void input(){
    gui::Host host;auto model=std::make_shared<gui::ColorPickerModel>();int edited=0,canceled=0;gui::Widget plane,rail,button;
    good(host.run({.opened=[&]{auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.hstack({},"root"));plane=take(b.color_surface(root,"plane",{.model=model,.field=gui::ColorField::saturation_brightness,.width=100,.height=100},[&](auto){++edited;}));rail=take(b.color_surface(root,"rail",{.model=model,.field=gui::ColorField::hue_rail,.width=18,.height=100}));button=take(b.combo_box(root,"oldPress",{}, {.canceled=[&]{++canceled;}}));return {};}));
        auto r=take(plane.snapshot()).bounds;
        auto old=take(button.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=old.x+5,.y=old.y+5}));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+75,.y=r.y+25}));
        check(near(take(model->state()).saturation,.75) && near(take(model->state()).brightness,.75) && edited==1 && canceled==1,"Pointer edit or previous control press cancellation failed");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=r.x+200,.y=r.y-10}));check(near(take(model->state()).saturation,1) && near(take(model->state()).brightness,1),"Drag capture did not clamp outside field");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+200,.y=r.y-10}));
        good(plane.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::left,.shift=true}));check(near(take(model->state()).saturation,.9),"Shift key channel step differs");
        good(rail.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));check(near(take(model->state()).hue,1),"Hue rail keyboard direction differs");
        good(plane.set_enabled(false));const auto before=take(model->state()).revision;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+20,.y=r.y+20}));check(take(model->state()).revision==before,"Disabled surface edited model");
        check(!plane.set_color_model({}) && !plane.set_color_field(static_cast<gui::ColorField>(99)),"Invalid color reconfiguration accepted");good(host.request_close());}}));
}
void lifetime(){
    gui::Host host;auto first=std::make_shared<gui::ColorPickerModel>(),second=std::make_shared<gui::ColorPickerModel>();gui::Widget field;int calls=0;
    good(host.run({.opened=[&]{take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{field=take(b.color_surface({},"field",{.model=first,.field=gui::ColorField::spectrum},[&](auto){++calls;}));return {};}));
        good(field.set_color_model(second));good(first->edit_color({1,0,0,1}));check(calls==0,"Rebound surface received old model changes");good(second->edit_color({0,1,0,1}));check(calls==1 && take(field.snapshot()).color_state->hex=="00FF00","Shared model edit did not update surface");
        good(host.unmount_widgets());good(second->edit_color({0,0,1,1}));check(calls==1 && !field.color_model(),"Unmount retained color callback or live handle");
        take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{take(b.color_surface({},"replacement",{.model=second},[&](auto){++calls;good(host.unmount_widgets());}));return {};}));
        good(second->edit_color({1,0,0,1}));check(calls==2,"Self-unmount callback did not finish safely");good(host.request_close());}}));
}
void rendering(){
    gui::Host host({.size={400,300}});auto model=std::make_shared<gui::ColorPickerModel>();gui::Widget field;
    good(model->edit_hsv(0,.2,.8));good(host.run({.opened=[&]{take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root",{.padding=20}));field=take(b.color_surface(root,"plane",{.model=model,.field=gui::ColorField::saturation_brightness,.width=120,.height=100}));return {};}));
        auto pixels=take(host.capture_canvas());auto info=take(host.graphics_info());auto r=take(field.snapshot()).bounds;const auto scale=double(pixels.width)/400;
        const auto index=(std::size_t((r.y+10)*scale)*pixels.width+std::size_t((r.x+r.width-10)*scale))*4;
        check(pixels.rgba[index]>180 && pixels.rgba[index+1]<70 && pixels.rgba[index+2]<70,"GPU color plane red corner differs");
        const auto count=take(field.snapshot()).color_domain_rasterizations;good(model->edit_hsv(0,.7,.3));(void)take(host.capture_canvas());
        check(take(field.snapshot()).color_domain_rasterizations==count,"Cursor-only change rebuilt color domain");
        good(model->edit_hsv(120,.7,.3));(void)take(host.capture_canvas());check(take(field.snapshot()).color_domain_rasterizations==count+1,"Dependent hue change did not rebuild domain");
        for(auto kind:{gui::ColorField::wheel,gui::ColorField::hue_saturation,gui::ColorField::spectrum,gui::ColorField::hue_rail,gui::ColorField::brightness_rail}){good(field.set_color_field(kind));(void)take(host.capture_canvas());}
        check(info.api==gui::GraphicsApi::metal && info.hardware,"Color field did not use native GPU surface");good(host.request_close());}}));
}
void native_events(){
    gui::Host host;auto model=std::make_shared<gui::ColorPickerModel>();gui::Widget field;bool found=false;
    good(host.run({.opened=[&]{take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{field=take(b.color_surface({},"color",{.model=model,.field=gui::ColorField::hue_saturation,.width=200,.height=180}));return {};}));
        for(NSWindow* w in NSApplication.sharedApplication.windows)if(w.visible && surface(w.contentView)){auto view=surface(w.contentView);no_controls(w.contentView);auto r=take(field.snapshot()).bounds;auto point=[view convertPoint:NSMakePoint(r.x+r.width/2,r.y+r.height/2) toView:nil];
            auto event=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:point modifierFlags:0 timestamp:0 windowNumber:w.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];[view mouseDown:event];
            check(near(take(model->state()).hue,180) && near(take(model->state()).saturation,.5),"Native color pointer event did not reach C++ model");
            for(NSAccessibilityElement* peer in view.accessibilityChildren)if([peer.accessibilityIdentifier isEqualToString:@"color"]){found=true;check([peer.accessibilityRole isEqualToString:NSAccessibilityColorWellRole] && [peer.accessibilityValue isEqual:@"#80FFFF"],"Virtual color accessibility value differs");check([peer accessibilityPerformPress] && ![peer accessibilityPerformIncrement],"Color accessibility focus/plane action differs");good(field.set_color_field(gui::ColorField::hue_rail));check([peer accessibilityPerformIncrement] && near(take(model->state()).hue,181),"Color rail accessibility increment failed");check([peer accessibilityPerformDecrement] && near(take(model->state()).hue,180),"Color rail accessibility decrement failed");}
        }check(found,"Color virtual accessibility element absent");good(host.request_close());}}));
}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"input";if(name=="input")input();else if(name=="lifetime")lifetime();else if(name=="rendering")rendering();else if(name=="native_events")native_events();else throw std::runtime_error("Unknown native color case");std::cout<<"NATIVE_COLOR_OK "<<name<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
