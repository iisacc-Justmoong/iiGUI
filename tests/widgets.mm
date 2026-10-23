#import <AppKit/AppKit.h>
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(core::Result<void> value) {
    if (value) return;
    if (value.error().cause) std::rethrow_exception(value.error().cause);
    throw std::runtime_error(value.error().message);
}
template<class T> bool closed(const core::Result<T>& value) { return !value && value.error().code == core::ErrorCode::closed; }
gui::WidgetMount simple(gui::Host& host) {
    return take(host.mount_widgets([](gui::WidgetBuilder& builder) -> core::Result<void> {
        auto stack = builder.vstack({}, "root", {.spacing=12,.padding=24});
        if (!stack) return std::unexpected(stack.error());
        auto label = builder.label(*stack,"message",{.text="Hello World",.font_size=28});
        if (!label) return std::unexpected(label.error());
        return {};
    }));
}
NSView* find_native(NSView* root, NSString* identifier) {
    if ([root.identifier isEqualToString:identifier]) return root;
    for (NSView* child in root.subviews) if (auto* found = find_native(child,identifier)) return found;
    return nil;
}
void lifecycle() {
    gui::Host host;
    gui::WidgetMount mounted;
    gui::Widget text;
    check(closed(text.snapshot()),"Empty widget must be closed");
    check(closed(host.mount_widgets({})),"Cannot mount before opening");
    bool closed_callback = false;
    good(host.run({.opened=[&] {
        mounted = simple(host);
        auto root = take(mounted.find("root")); text = take(mounted.find("message"));
        auto snapshots = take(mounted.snapshot());
        check(snapshots.size()==2 && snapshots[0].attached && snapshots[1].attached,"Native tree was not attached");
        NSView* surface=nil;
        for(NSWindow* w in NSApplication.sharedApplication.windows) if(w.visible) {
            surface=find_native(w.contentView,@"iiGUI.widget.surface");
            if(surface) break;
        }
        check(surface && std::string(NSStringFromClass(surface.class).UTF8String)=="IIGUIWidgetSurface","Missing iiGUI input surface");
        std::function<bool(NSView*)> forbidden=[&](NSView* v) {
            if([v isKindOfClass:NSControl.class] || [v isKindOfClass:NSStackView.class]) return true;
            for(NSView* child in v.subviews) if(forbidden(child)) return true;
            return false;
        };
        check(!forbidden(surface.window.contentView),"OS control or OS layout created for a C++ widget");
        check(snapshots[1].native_class=="iiGUI.Label" && snapshots[1].renderer=="iiGUI","Label is not self-rendered");
        const auto before = take(root.snapshot()).size.width;
        good(host.set_content_size({820,520}));
        const auto after = take(root.snapshot()).size.width;
        check(before > 0 && after > before + 100 && std::abs(after-820)<1,"iiGUI layout did not follow resize");
        good(text.set_text("Hello Cocoa · 한글\nUTF-8"));
        check(take(text.snapshot()).text=="Hello Cocoa · 한글\nUTF-8","Native text update failed");
        std::thread worker([&] {
            auto mutation = text.set_text("wrong");
            check(!mutation && mutation.error().code==core::ErrorCode::wrong_executor,"Worker native mutation accepted");
            auto access = mounted.snapshot();
            check(!access && access.error().code==core::ErrorCode::wrong_executor,"Worker snapshot accepted");
        }); worker.join();
        check(take(host.graphics_info()).api==gui::GraphicsApi::metal,"Widget tree did not use the iiGUI GPU renderer");
        good(host.request_close());
    },.closed=[&] {
        closed_callback=true;
        check(closed(text.set_text("late")) && closed(mounted.snapshot()),"Close did not revoke widget handles before notifying app");
    }}));
    check(closed_callback && closed(text.snapshot()),"Native cleanup failed");
}
void rollback() {
    gui::Host host;
    good(host.run({.opened=[&] {
        auto mounted=simple(host); auto old=take(mounted.find("message"));
        auto legacy_text=host.set_text("hidden");
        check(!legacy_text && legacy_text.error().code==core::ErrorCode::busy,"Legacy text silently replaced mounted content");
        gui::Widget failed;
        auto rejected=host.mount_widgets([&](gui::WidgetBuilder& builder) -> core::Result<void> {
            failed=take(builder.label({},"new",{.text="Uncommitted"}));
            return core::fail(core::ErrorCode::failed,"Deliberate rollback");
        });
        check(!rejected && closed(failed.snapshot()),"Rejected factory leaked a usable widget");
        check(take(old.snapshot()).attached,"Rollback removed old content");
        auto thrown=host.mount_widgets([](gui::WidgetBuilder&) -> core::Result<void> { throw std::runtime_error("factory"); });
        check(!thrown && thrown.error().cause,"Factory exception not returned");
        check(!host.mount_widgets([](gui::WidgetBuilder&) -> core::Result<void> { return {}; }),"Empty tree accepted");
        auto duplicate=host.mount_widgets([](gui::WidgetBuilder& b) -> core::Result<void> {
            auto root=take(b.vstack({},"same"));
            auto bad=b.label(root,"same",{});
            return bad ? core::Result<void>{} : std::unexpected(bad.error());
        });
        check(!duplicate,"Duplicate native id accepted");
        auto foreign=host.mount_widgets([&](gui::WidgetBuilder& b) -> core::Result<void> {
            auto result=b.label(old,"bad",{});
            return result ? core::Result<void>{} : std::unexpected(result.error());
        });
        check(!foreign,"Parent from another generation accepted");
        auto invalid=host.mount_widgets([](gui::WidgetBuilder& b) -> core::Result<void> {
            auto result=b.vstack({},"bad",{.spacing=std::numeric_limits<double>::quiet_NaN()});
            return result ? core::Result<void>{} : std::unexpected(result.error());
        });
        check(!invalid,"NaN native geometry accepted");
        check(take(old.snapshot()).text=="Hello World","Failed transactions changed old widgets");
        good(host.unmount_widgets()); check(closed(old.snapshot()),"Unmount did not invalidate generation");
        auto replacement=simple(host); check(take(replacement.find("message")).snapshot().has_value(),"Replacement mount failed");
        check(closed(old.set_text("stale")),"Old handle modified a replacement");
        good(host.request_close());
    }}));
}
void ownership() {
    struct Owner { int& count; ~Owner() { ++count; } };
    int destroyed=0,clicks=0;
    gui::Host host; gui::Widget button;
    good(host.run({.opened=[&] {
        auto result=host.mount_widgets([&](gui::WidgetBuilder& b) -> core::Result<void> {
            auto owner=std::make_shared<Owner>(destroyed);
            good(b.retain(owner));
            button=take(b.button({},"click",{.text="Click"},[&] { ++clicks; }));
            return {};
        });
        take(std::move(result));
        check(destroyed==0,"Uncaptured scope died before unmount");
        good(button.activate()); good(button.activate());
        check(clicks==2 && destroyed==0,"Repeated native actions recreated/destroyed the owner");
        good(button.set_enabled(false)); good(button.activate());
        check(clicks==2 && !take(button.snapshot()).enabled,"Disabled native button invoked action");
        good(host.unmount_widgets());
        check(destroyed==1 && closed(button.activate()),"Scope was not released exactly once");
        good(host.request_close());
    }}));
    check(destroyed==1,"Scope destroyed twice");
}
void reentrant() {
    gui::Host host; gui::Widget button;
    good(host.run({.opened=[&] {
        take(host.mount_widgets([&](gui::WidgetBuilder& b) -> core::Result<void> {
            auto nested=host.mount_widgets([](gui::WidgetBuilder&) -> core::Result<void> { return {}; });
            check(!nested && nested.error().code==core::ErrorCode::busy,"Reentrant mount accepted");
            button=take(b.button({},"remove",{.text="Remove"},[&] { good(host.unmount_widgets()); }));
            return {};
        }));
        good(button.activate()); check(closed(button.snapshot()),"Self-unmount action retained native controls");
        good(host.request_close());
    }}));
}
void failure() {
    gui::Host host; gui::Widget button;
    auto result=host.run({.opened=[&] {
        take(host.mount_widgets([&](gui::WidgetBuilder& b) -> core::Result<void> {
            button=take(b.button({},"failure",{.text="Fail"},[] { throw std::runtime_error("native action"); }));
            return {};
        }));
        good(button.activate());
    }});
    check(!result && result.error().cause && closed(button.snapshot()),"Native action exception escaped its host barrier");
}
void content_switch() {
    gui::Host host;
    good(host.run({.opened=[&] {
        auto mounted=simple(host); auto old=take(mounted.find("message"));
        gui::CanvasFrame invalid; invalid.viewport={0,0,-1,120};
        check(!host.set_canvas(invalid) && take(old.snapshot()).attached,"Rejected canvas destroyed mounted widgets");
        gui::CanvasFrame frame; frame.viewport={0,0,160,120}; frame.renderer=gui::Renderer::native;
        good(host.set_canvas(frame));
        check(closed(old.snapshot()) && take(host.graphics_info()).api==gui::GraphicsApi::metal,"Canvas did not replace widgets");
        auto failed=host.mount_widgets([](gui::WidgetBuilder&) -> core::Result<void> {
            return core::fail(core::ErrorCode::failed,"rollback to canvas");
        });
        check(!failed && take(host.graphics_info()).api==gui::GraphicsApi::metal,"Failed mount discarded the canvas");
        auto replacement=simple(host);
        check(take(host.graphics_info()).api==gui::GraphicsApi::metal && take(replacement.snapshot()).size()==2,"Self-rendered mount did not replace canvas");
        good(host.request_close());
    }}));
}
void lvrs_appearance() {
    gui::Host host;
    good(host.run({.opened=[&] {
        auto appearance=gui::lvrs_appearance();
        check(std::abs(appearance.window.red-20.0/255)<1e-8 && std::abs(appearance.window.blue-25.0/255)<1e-8,"LVRS 2% window accent mix changed");
        auto mounted=take(host.mount_widgets([](gui::WidgetBuilder& b) -> core::Result<void> {
            auto root=take(b.vstack({},"styled-root",{.spacing=8,.padding=24}));
            take(b.label(root,"styled-label",{.text="LVRS",.font_size=26,.role=gui::TextRole::title}));
            take(b.button(root,"styled-button",{.text="Native Button"})); return {};
        },appearance));
        auto handle=take(mounted.find("styled-button"));
        auto label=take(take(mounted.find("styled-label")).snapshot());
        auto button=take(handle.snapshot());
        check(button.native_class=="iiGUI.Button" && label.native_class=="iiGUI.Label","OS widgets substituted for iiGUI widgets");
        check(std::abs(button.size.height-22)<.1 && button.font_size==13 && label.font_size==26,"LVRS widget metrics changed");
        auto pixel=[&] {
            auto image=take(host.capture_canvas()); auto scale=take(host.snapshot()).backing_scale;
            auto x=std::size_t((button.bounds.x+3)*scale), y=std::size_t((button.bounds.y+button.bounds.height/2)*scale);
            auto offset=(y*image.width+x)*4;
            return std::array<unsigned,3>{image.rgba[offset],image.rgba[offset+1],image.rgba[offset+2]};
        };
        auto p=pixel(); check(p[2]>200 && p[1]>80 && p[0]<55,"LVRS primary did not reach GPU pixels");
        good(handle.set_enabled(false)); p=pixel();
        check(p[2]<55 && p[0]<55,"Disabled LVRS color did not reach GPU pixels");
        check(take(handle.snapshot()).styled && take(host.graphics_info()).hardware,"Widgets are not hardware rendered");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv) {
    try {
        std::string_view name=argc>1?argv[1]:"lifecycle";
        if (name=="lifecycle") lifecycle(); else if (name=="rollback") rollback();
        else if (name=="ownership") ownership(); else if (name=="reentrant") reentrant();
        else if (name=="failure") failure(); else if (name=="content_switch") content_switch();
        else if (name=="lvrs_appearance") lvrs_appearance();
        else throw std::runtime_error("Unknown test");
        std::cout<<"NATIVE_WIDGETS_OK "<<name<<'\n';
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
