#include <iiGUI/PaintView.h>

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace ii::gui;
using ii::core::Result;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
template<class T>T take(Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
struct Device final : FramePaintDevice {
    PaintCapabilities capabilities()const noexcept override{return {};}
    Result<void> paint_frame(const CanvasFrame&,const PaintFrameInfo&,std::stop_token)override{return {};}
};
}
int main(){
    try{
        ii::runtime::EventLoop events(16);ii::runtime::FrameLoop frames(events);
        int invalidations=0, geometry_updates=0, frame_starts=0, frame_stops=0, ticks=0;
        WindowGeometry last;
        PaintViewServices services{.events=&events,.frames=&frames,
            .invalidated=[&]{++invalidations;},
            .frame_activity=[&](bool active){active?++frame_starts:++frame_stops;},
            .window_geometry=[&](const WindowGeometry& geometry)->Result<void>{++geometry_updates;last=geometry;return {};}};
        auto view=take(PaintView::create(std::make_shared<Device>(),[&](WidgetBuilder& builder)->Result<void>{
            auto target=builder.window_properties();if(!target)return std::unexpected(target.error());
            WindowProperties properties;properties.geometry.title="Initial";
            if(auto result=target->update(properties);!result)return result;
            auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
            auto label=builder.label(*root,"label",{.text="Initial"});
            return label?Result<void>{}:std::unexpected(label.error());
        },{},services));
        check(geometry_updates==1 && last.title=="Initial", "Window geometry was not committed by the external host");
        check(take(view->needs_paint()) && invalidations==0,"A new view did not start dirty or notified synchronously");
        (void)events.poll();check(invalidations==1,"Initial invalidation was not delivered");
        good(view->paint({200,100}));check(!take(view->needs_paint()),"Successful paint did not clear the dirty state");
        auto label=take(view->find("label"));good(label.set_text("Changed"));good(label.set_text("Changed again"));
        check(invalidations==1 && take(view->needs_paint()),"Mutation did not mark the view dirty asynchronously");
        (void)events.poll();check(invalidations==2,"Changes were not coalesced into one notification");
        good(view->paint({200,100}));
        good(label.set_frame_handler([&](ii::runtime::Clock::time_point)->Result<bool>{++ticks;return false;},true));
        check(frame_starts==1 && ticks==0,"Frame activity ran without a host frame");
        (void)frames.step();check(ticks==1 && frame_stops==1,"Explicit host frame did not retire its handler");
        good(label.set_text("Pending"));good(view->close());
        const auto before=invalidations;(void)events.poll();
        check(invalidations==before,"Retired view delivered a queued invalidation");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
