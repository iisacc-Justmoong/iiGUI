#include <iiGUI/PaintView.h>
#include "../src/Platform/TouchInputBridge.h"
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Device final : gui::PaintDevice {
    gui::PaintCapabilities capabilities()const noexcept override{return {.api=gui::GraphicsApi::vulkan,.hardware=true,.native_api=true};}
    core::Result<void> begin_frame(const gui::PaintFrameInfo&)override{return {};}
    core::Result<void> begin_scene(const gui::PaintSceneInfo&)override{return {};}
    core::Result<void> end_scene()override{return {};}
    core::Result<void> begin_layer(const gui::PaintLayerInfo&)override{return {};}
    core::Result<void> draw_panels(std::span<const gui::Panel>)override{return {};}
    core::Result<void> draw_text(std::span<const gui::TextRun>)override{return {};}
    core::Result<void> draw_images(std::span<const gui::ImageRun>)override{return {};}
    core::Result<void> end_layer()override{return {};}
    core::Result<void> present()override{return {};}
    void abort_frame()noexcept override{}
};

struct Fixture final {
    runtime::EventLoop loop;
    std::shared_ptr<gui::PaintView> tree;
    gui::Widget host,surface,photo,menu;
    int clicks=0,cancellations=0;
    bool reenter=false;
    std::vector<gui::HorizontalDrag> drags;
    Fixture(){
        tree=take(gui::PaintView::create(std::make_shared<Device>(),[&](gui::WidgetBuilder& outer)->core::Result<void>{
            host=take(outer.content_host({},"host",{240,160}));
            return host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
                surface=take(b.absolute_stack({},"surface",{240,160}));
                auto nested=take(b.content_host(surface,"nested",{200,120}));
                good(nested.set_frame({0,0,200,120}));
                good(nested.set_content([&](gui::WidgetBuilder& inner)->core::Result<void>{
                    auto root=take(inner.absolute_stack({},"inner",{200,120}));
                    photo=take(inner.button(root,"photo",{.text="Photo",.height=60,.width=100},[&]{++clicks;}));
                    good(photo.set_frame({20,20,100,60}));menu=take(inner.menu(root,"menu",{.item_width=100}));
                    good(photo.set_control_events({.canceled=[&]{
                        ++cancellations;
                        if(std::exchange(reenter,false)){
                            send(gui::WidgetInputKind::pointer_down,30,30);
                            send(gui::WidgetInputKind::pointer_up,30,30);
                        }
                    }}));
                    return photo.set_context_menu(menu);
                }));
                return surface.set_horizontal_drag_handler({.threshold=6},[&](const gui::HorizontalDrag& drag)->core::Result<void>{drags.push_back(drag);return {};});
            });
        },{},gui::PaintViewServices{.events=&loop}));
        good(tree->paint({240,160}));
    }
    ~Fixture(){(void)tree->close();}
    void send(gui::WidgetInputKind kind,double x,double y,gui::PointerDevice device=gui::PointerDevice::mouse,std::uint32_t buttons=1){
        good(tree->input({.kind=kind,.x=x,.y=y,.device=device,.device_id=7,.buttons=buttons}));
    }
};
void pointer_and_lifetime(){
    Fixture f;
    f.send(gui::WidgetInputKind::pointer_down,30,30);check(take(f.photo.press_origin())==gui::WidgetPressOrigin::pointer,"Photo did not receive the original press");f.send(gui::WidgetInputKind::pointer_up,30,30);
    check(f.clicks==1&&f.drags.empty(),"Click was intercepted by a pending drag");
    f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,33,30);
    check(f.drags.empty(),"Motion below the threshold started a drag");
    f.send(gui::WidgetInputKind::pointer_move,62,31);
    check(f.drags.size()==1&&std::abs(f.drags[0].delta_x-32)<1e-9&&
          std::abs(f.drags[0].total_x-32)<1e-9&&f.drags[0].first&&
          std::abs(f.drags[0].origin.x-30)<1e-9&&std::abs(f.drags[0].origin.y-30)<1e-9,
          "Horizontal drag did not report its initial distance and press location");
    check(f.cancellations==1&&take(f.photo.press_origin())==gui::WidgetPressOrigin::none,
          "The original nested press was not canceled on drag capture");
    f.send(gui::WidgetInputKind::pointer_move,300,31);f.send(gui::WidgetInputKind::pointer_up,300,31);
    check(f.drags.size()==2&&std::abs(f.drags[1].delta_x-238)<1e-9&&
          std::abs(f.drags[1].total_x-270)<1e-9&&!f.drags[1].first&&f.clicks==1,
          "Captured drag was lost outside the host or activated a photo");
    f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_up,30,30);
    check(f.clicks==2,"Drag capture survived its release");
    f.drags.clear();f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,31,45);
    f.send(gui::WidgetInputKind::pointer_move,75,45);f.send(gui::WidgetInputKind::pointer_up,75,45);
    check(f.drags.empty(),"Vertical-first movement became a horizontal drag");
    f.send(gui::WidgetInputKind::pointer_down,30,30,gui::PointerDevice::mouse,2);
    f.send(gui::WidgetInputKind::pointer_move,70,30,gui::PointerDevice::mouse,2);
    f.send(gui::WidgetInputKind::pointer_cancel,70,30,gui::PointerDevice::mouse,2);
    check(f.drags.empty(),"A secondary button started a horizontal drag");
    f.send(gui::WidgetInputKind::pointer_down,30,30);good(f.surface.set_visible(false));
    good(f.surface.set_visible(true));f.send(gui::WidgetInputKind::pointer_move,70,30);f.send(gui::WidgetInputKind::pointer_up,70,30);
    check(f.drags.empty(),"A hidden surface retained a pending drag");
    const auto before=f.clicks;f.reenter=true;f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,70,30);
    check(!f.reenter&&f.drags.empty()&&f.clicks==before+1,"Cancellation replaced a newer reentrant press");
}
void mobile_router(){
    Fixture f;runtime::touch::TouchRouter router;
    const auto deliver=[&](const std::optional<runtime::touch::TouchBatch>& batch){
        check(bool(batch),"Touch router rejected a valid sample");
        for(std::size_t i=0;i<batch->size;++i)if(auto event=gui::detail::widget_touch_event((*batch)[i]))good(f.tree->input(*event));
    };
    runtime::touch::TouchSample p{.identity=7,.x=30,.y=30,.timestamp=1};
    deliver(router.down(p,true));p.x=40;p.timestamp=1.02;deliver(router.move(p));
    check(f.drags.size()==1&&std::abs(f.drags[0].delta_x-10)<1e-9&&f.loop.stats().pending==0,
          "Horizontal touch replay did not claim and cancel the long press");
    p.x=65;p.timestamp=1.03;deliver(router.move(p));deliver(router.up(p));
    check(f.drags.size()==2&&f.clicks==0,"Touch drag activated its photo");
    f.drags.clear();p={.identity=8,.x=30,.y=30,.timestamp=2};deliver(router.down(p,true));
    p.y=45;p.timestamp=2.02;deliver(router.move(p));deliver(router.up(p));
    check(f.drags.empty(),"Vertical touch scroll was interpreted as a drag");
}
void cancellation(){
    Fixture f;
    f.send(gui::WidgetInputKind::pointer_down,30,30);good(f.host.set_visible(false));good(f.host.set_visible(true));
    f.send(gui::WidgetInputKind::pointer_move,70,30);f.send(gui::WidgetInputKind::pointer_up,70,30);
    check(f.drags.empty(),"An invisible embedding host retained its pending gesture");
    f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,70,30);
    check(f.drags.size()==1,"An embedding host did not recover after hiding");
    good(f.host.set_visible(false));good(f.host.set_visible(true));
    f.send(gui::WidgetInputKind::pointer_move,90,30);f.send(gui::WidgetInputKind::pointer_up,90,30);
    check(f.drags.size()==1,"A hidden embedding host continued an active gesture");
    f.drags.clear();
    f.send(gui::WidgetInputKind::pointer_down,30,30,gui::PointerDevice::touch);
    auto deadline=f.loop.next_deadline();check(deadline&&f.loop.stats().pending==1,"Touch did not schedule its context menu");
    std::this_thread::sleep_until(*deadline+std::chrono::milliseconds(10));f.loop.poll();
    check(take(f.menu.snapshot()).opened,"Long press did not open its context menu");
    f.send(gui::WidgetInputKind::pointer_move,70,30,gui::PointerDevice::touch);
    f.send(gui::WidgetInputKind::pointer_up,70,30,gui::PointerDevice::touch);
    check(f.drags.empty(),"A completed long press was replaced by a drag");
    good(f.menu.close_popup());
    check(!f.surface.set_horizontal_drag_handler({.threshold=0},{}),"An invalid threshold replaced the active policy");
    f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,70,30);
    check(f.drags.size()==1,"Rejecting a policy changed the existing handler");
    good(f.tree->input({.kind=gui::WidgetInputKind::focus_lost}));
    f.send(gui::WidgetInputKind::pointer_move,90,30);f.send(gui::WidgetInputKind::pointer_up,90,30);
    check(f.drags.size()==1,"A focus loss retained drag capture");
    good(f.surface.set_horizontal_drag_handler({},[](const gui::HorizontalDrag&)->core::Result<void>{return core::fail(core::ErrorCode::failed,"drag failed");}));
    f.send(gui::WidgetInputKind::pointer_down,30,30);
    const auto failure=f.tree->input({.kind=gui::WidgetInputKind::pointer_move,.x=70,.y=30,.device_id=7,.buttons=1});
    check(!failure&&failure.error().code==core::ErrorCode::failed,"A drag callback failure was lost");
    f.send(gui::WidgetInputKind::pointer_up,70,30);
    good(f.surface.set_horizontal_drag_handler({},{}));
    const auto before=f.clicks;f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_up,30,30);
    check(f.clicks==before+1,"Removing the drag handler broke ordinary clicks");
    good(f.surface.set_horizontal_drag_handler({},[&](const gui::HorizontalDrag&)->core::Result<void>{good(f.host.clear_content());return {};}));
    f.send(gui::WidgetInputKind::pointer_down,30,30);f.send(gui::WidgetInputKind::pointer_move,70,30);
    check(!f.surface.snapshot(),"A drag callback could not retire its owning view");
}
}
int main(){pointer_and_lifetime();mobile_router();cancellation();}
