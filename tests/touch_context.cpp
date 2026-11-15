#include "../src/Widgets/WidgetState.h"
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
template<class T>T take(core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
}
int main(){
    runtime::EventLoop loop;
    gui::detail::WidgetFrameHooks hooks;hooks.events=[&]()->core::Result<runtime::EventLoop*>{return &loop;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"host",{240,160}));gui::Widget button,menu;int clicks=0;
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"root",{240,160}));
        button=take(b.button(root,"photo",{.text="Photo",.height=60,.width=100},[&]{++clicks;}));
        good(button.set_frame({20,20,100,60}));menu=take(b.menu(root,"menu",{.item_width=100}));
        return button.set_context_menu(menu);
    }));
    (void)take(tree->drawing_snapshot({240,160}));
    const auto send=[&](gui::WidgetInputKind kind,double x=30,double y=30,gui::PointerDevice device=gui::PointerDevice::touch){
        good(tree->input({.kind=kind,.x=x,.y=y,.device=device,.device_id=7}));
    };
    send(gui::WidgetInputKind::pointer_down,30,30,gui::PointerDevice::mouse);
    check(loop.stats().pending==0,"Mouse press scheduled a touch deadline");
    send(gui::WidgetInputKind::pointer_up,30,30,gui::PointerDevice::mouse);clicks=0;
    send(gui::WidgetInputKind::pointer_down);
    check(loop.stats().pending==1,"Touch press did not schedule one context deadline");
    auto deadline=loop.next_deadline();check(bool(deadline),"Context deadline was not exposed to the host");
    std::this_thread::sleep_until(*deadline+std::chrono::milliseconds(10));loop.poll();
    check(take(menu.snapshot()).opened&&clicks==0,"Long touch did not open its context menu");
    send(gui::WidgetInputKind::pointer_up);check(clicks==0,"Long touch also activated the photo");
    good(menu.close_popup());
    send(gui::WidgetInputKind::pointer_down);send(gui::WidgetInputKind::pointer_up);
    check(loop.stats().pending==0&&!take(menu.snapshot()).opened,"Ordinary tap retained its deadline");
    send(gui::WidgetInputKind::pointer_down);send(gui::WidgetInputKind::pointer_move,60,30);
    check(loop.stats().pending==0,"Moving the touch did not cancel the context deadline");
    send(gui::WidgetInputKind::pointer_cancel,60,30);
    send(gui::WidgetInputKind::pointer_down);send(gui::WidgetInputKind::scroll);
    check(loop.stats().pending==0,"Scrolling an embedded view did not cancel the context deadline");
    send(gui::WidgetInputKind::pointer_cancel);
    send(gui::WidgetInputKind::pointer_down);good(button.set_enabled(false));
    check(loop.stats().pending==0,"Disabling the target retained its context deadline");
    good(button.set_enabled(true));send(gui::WidgetInputKind::pointer_cancel);
    send(gui::WidgetInputKind::pointer_down);good(button.set_visible(false));
    check(loop.stats().pending==0,"Hiding the target retained its context deadline");
    good(button.set_visible(true));send(gui::WidgetInputKind::pointer_cancel);
    send(gui::WidgetInputKind::pointer_down);good(button.set_context_menu({}));
    check(loop.stats().pending==0,"Removing the context binding retained its deadline");
    send(gui::WidgetInputKind::pointer_cancel);send(gui::WidgetInputKind::pointer_down);
    check(loop.stats().pending==0,"Touch without a context binding scheduled a deadline");
    send(gui::WidgetInputKind::pointer_cancel);good(button.set_context_menu(menu));
    send(gui::WidgetInputKind::pointer_down);good(host.set_visible(false));
    check(loop.stats().pending==0,"Hiding an embedding host retained its child's context deadline");
    good(host.set_visible(true));send(gui::WidgetInputKind::pointer_cancel);
    good(button.set_touch_context_options({.delay=std::chrono::seconds(3),.movement_tolerance=20}));
    check(take(button.touch_context_options()).delay==std::chrono::seconds(3),"Context options could not be queried");
    check(!button.set_touch_context_options({.delay=std::chrono::milliseconds(-1)})&&
          take(button.touch_context_options()).delay==std::chrono::seconds(3),"Invalid context options replaced the live policy");
    const auto started=runtime::Clock::now();send(gui::WidgetInputKind::pointer_down);
    auto custom=loop.next_deadline();check(custom&&*custom>=started+std::chrono::seconds(3),"Custom context delay was ignored");
    send(gui::WidgetInputKind::pointer_move,45,30);check(loop.stats().pending==1,"Custom movement tolerance was ignored");
    send(gui::WidgetInputKind::pointer_cancel);good(button.set_touch_context_options({.enabled=false}));
    send(gui::WidgetInputKind::pointer_down);check(loop.stats().pending==0,"Disabled touch context scheduled a deadline");
    send(gui::WidgetInputKind::pointer_cancel);good(button.set_touch_context_options({}));
    send(gui::WidgetInputKind::pointer_down);good(host.clear_content());
    check(loop.stats().pending==0,"Retiring the target retained its context deadline");
    tree->close();

    std::shared_ptr<gui::detail::WidgetState> reentrant;
    gui::detail::WidgetFrameHooks closingHooks;
    closingHooks.events=[&]()->core::Result<runtime::EventLoop*>{reentrant->close();return &loop;};
    reentrant=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},closingHooks);
    auto closingHost=take(reentrant->content_host({},"host",{240,160}));
    good(closingHost.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"root",{240,160}));
        auto photo=take(b.button(root,"photo",{.text="Photo",.height=60,.width=100}));
        good(photo.set_frame({20,20,100,60}));auto context=take(b.menu(root,"menu",{.item_width=100}));
        return photo.set_context_menu(context);
    }));
    (void)take(reentrant->drawing_snapshot({240,160}));
    (void)reentrant->input({.kind=gui::WidgetInputKind::pointer_down,.x=30,.y=30,
                            .device=gui::PointerDevice::touch,.device_id=1});
    check(!reentrant->ready()&&loop.stats().pending==0,"A closing event-loop provider scheduled work after retirement");
}
