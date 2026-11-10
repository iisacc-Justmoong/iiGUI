#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include "../src/Platform/AndroidPointer.h"
#include "../src/Platform/TouchInputBridge.h"
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>

using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct MotionSurface final:gui::detail::WidgetSurface {
    bool& reduced;
    gui::detail::WidgetSurfaceCallbacks callbacks;
    MotionSurface(bool& value,gui::detail::WidgetSurfaceCallbacks input):reduced(value),callbacks(std::move(input)){}
    void invalidate()override{} void accessibility_changed()override{} void accessibility_value_changed(std::string_view)override{}
    void set_frame_paced(bool)override{} void flush()override{}
    gui::Size size()const override{return {200,120};} double backing_scale()const override{return 1.;}
    std::string resolve_font_family(std::string_view value)const override{return std::string(value);}
    gui::GraphicsInfo info()override{return {};} gui::PixelBuffer capture()override{return {};}
    bool native_event(const void*)override{return false;}
    bool reduced_motion()const override{return reduced;}
};
struct Fixture final {
    runtime::EventLoop events;
    runtime::FrameLoop frames{events};
    std::shared_ptr<gui::MotionPreferences> preferences=std::make_shared<gui::MotionPreferences>();
    std::shared_ptr<gui::detail::WidgetState> tree;
    gui::Widget host,scroll;
    MotionSurface* surface=nullptr;
    bool activity=false;
    explicit Fixture(bool* system_reduced=nullptr){
        gui::detail::WidgetFrameHooks hooks;
        hooks.frames=[&]()->core::Result<runtime::FrameLoop*>{return &frames;};
        hooks.activity=[&](bool value){activity=value;};hooks.preferences=preferences;
        tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
        host=take(tree->content_host({},"host",{200,120}));
        good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
            scroll=take(b.scroll_view({},"scroll",{.width=200,.height=120,.virtual_content_height=1200}));return {};
        }));
        if(system_reduced)good(tree->attach(nullptr,[this,system_reduced](void*,gui::detail::WidgetSurfaceCallbacks callbacks){
            auto result=std::make_unique<MotionSurface>(*system_reduced,std::move(callbacks));surface=result.get();return result;
        }));
        (void)take(tree->drawing_snapshot({200,120}));
    }
    ~Fixture(){tree->close();}
    void drag(double time){
        good(tree->touch_begin(time));
        good(tree->input({.kind=gui::WidgetInputKind::scroll,.x=50,.y=50,.scroll_y=-20,.precise_scroll=true,.device=gui::PointerDevice::touch,.timestamp=time+.02}));
        good(tree->input({.kind=gui::WidgetInputKind::scroll,.x=50,.y=50,.scroll_y=-20,.precise_scroll=true,.device=gui::PointerDevice::touch,.timestamp=time+.04}));
    }
};
void reentrant_restart(){
    runtime::EventLoop events;runtime::FrameLoop frames{events};bool active=false,restart=false;double momentum_time=0;
    gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->core::Result<runtime::FrameLoop*>{return &frames;};
    hooks.activity=[&](bool value){active=value;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{100,100}));
    const auto pan=[&](double timestamp){good(tree->input({.kind=gui::WidgetInputKind::scroll,.x=40,.y=40,
        .scroll_x=20,.precise_scroll=true,.device=gui::PointerDevice::touch,.timestamp=timestamp}));};
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"canvas",{100,100}));
        return root.set_input_handler([&](const gui::WidgetInput& input)->core::Result<bool>{
            if(input.kind!=gui::WidgetInputKind::scroll)return false;
            if(restart){restart=false;momentum_time=input.timestamp;good(tree->touch_begin(2.));pan(2.02);pan(2.04);good(tree->touch_end(2.04));return false;}
            return true;
        });
    }));
    (void)take(tree->drawing_snapshot({100,100}));
    good(tree->touch_begin(1.));pan(1.02);pan(1.04);good(tree->touch_end(1.04));
    check(active,"initial canvas fling did not start");restart=true;
    (void)frames.step(runtime::Clock::now()+40ms);
    check(active,"old frame canceled a reentrantly started fling");
    check(momentum_time>=1.04,"inertial scroll lost its event timestamp");tree->close();
}
void nested_axes(){
    runtime::EventLoop events;runtime::FrameLoop frames{events};gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->core::Result<runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{240,180}));gui::Widget outer,inner;
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        outer=take(b.scroll_view({},"outer",{.width=200,.height=150,.virtual_content_height=600}));
        inner=take(b.scroll_view(outer,"inner",{.width=100,.height=80,.virtual_content_height=80,.virtual_content_width=400}));
        return {};
    }));
    (void)take(tree->drawing_snapshot({240,180}));good(tree->touch_begin(1.));
    for(const auto time:{1.02,1.04})good(tree->input({.kind=gui::WidgetInputKind::scroll,.x=40,.y=40,
        .scroll_y=-20,.scroll_x=-10,.precise_scroll=true,.device=gui::PointerDevice::touch,.timestamp=time}));
    const auto x=take(inner.snapshot()).horizontal_scroll_offset,y=take(outer.snapshot()).scroll_offset;
    good(tree->touch_end(1.04));(void)frames.step(runtime::Clock::now()+40ms);
    check(take(inner.snapshot()).horizontal_scroll_offset>x&&take(outer.snapshot()).scroll_offset>y,
          "inertia lost an axis across nested scroll views");tree->close();
}
}
int main(){
    Fixture f;
    f.drag(1.);const auto direct=take(f.scroll.snapshot()).scroll_offset;
    check(std::nearbyint(direct)==40&&!f.activity,"touch moved after release before it ended");
    good(f.tree->touch_end(1.04));check(f.activity,"release did not request display frames");
    const auto now=runtime::Clock::now();(void)f.frames.step(now+40ms);
    const auto moving=take(f.scroll.snapshot()).scroll_offset;
    check(moving>direct,"content stopped immediately after release");
    good(f.tree->touch_begin(2.));check(!f.activity,"new contact did not stop inertia");
    (void)f.frames.step(now+100ms);check(take(f.scroll.snapshot()).scroll_offset==moving,"canceled motion moved again");

    f.drag(2.2);good(f.tree->touch_end(2.24));const auto before_preference=take(f.scroll.snapshot()).scroll_offset;
    good(f.preferences->set_reduced_motion(true));(void)f.frames.step(now+140ms);
    check(!f.activity&&take(f.scroll.snapshot()).scroll_offset==before_preference,"reduced motion did not stop an active fling");
    good(f.preferences->set_reduced_motion(false));
    f.drag(3.);good(f.tree->touch_cancel());(void)f.frames.step(now+160ms);
    check(!f.activity,"canceled gesture started inertia");
    good(f.preferences->set_reduced_motion(true));f.drag(4.);good(f.tree->touch_end(4.04));
    check(!f.activity,"reduced motion still started inertia");
    good(f.preferences->set_reduced_motion(false));
    good(f.scroll.set_scroll_offset(1079));f.drag(5.);good(f.tree->touch_end(5.04));
    (void)f.frames.step(now+200ms);(void)f.frames.step(now+240ms);
    check(take(f.scroll.snapshot()).scroll_offset==1080&&!f.activity,"inertia continued at the scroll boundary");

    f.drag(6.);good(f.tree->touch_end(6.04));good(f.host.clear_content());
    (void)f.frames.step(now+280ms);(void)f.frames.step(now+320ms);
    check(!f.activity,"removed content kept its fling alive");
    reentrant_restart();
    nested_axes();
    bool system_reduced=true;Fixture system(&system_reduced);
    system.drag(10.);good(system.tree->touch_end(10.04));check(!system.activity,"system reduced motion started inertia");
    system_reduced=false;system.drag(11.);good(system.tree->touch_end(11.04));check(system.activity,"system motion setting did not allow inertia");
    system_reduced=true;(void)system.frames.step(runtime::Clock::now()+40ms);
    check(!system.activity,"system reduced motion did not stop an active fling");
    system_reduced=false;
    auto& callbacks=system.surface->callbacks;runtime::touch::TouchRouter router;
    runtime::touch::TouchSample point;point.identity=1;point.x=50;point.y=90;point.timestamp=12.;
    check(callbacks.touch_scrollable(point.x,point.y),"native scroll candidate was not detected");
    callbacks.touch_begin(point.timestamp);
    auto deliver=[&](const std::optional<runtime::touch::TouchBatch>& batch){
        check(bool(batch),"native touch sample was rejected");
        for(std::size_t i=0;i<batch->size;++i)if(auto value=gui::detail::widget_touch_event((*batch)[i]))callbacks.input(*value);
    };
    deliver(router.down(point,true));point.y=70;point.timestamp=12.02;deliver(router.move(point));
    point.y=50;point.timestamp=12.04;deliver(router.move(point));
    const auto before_release=take(system.scroll.snapshot()).scroll_offset;
    deliver(router.up(point));check(router.empty(),"native touch did not end");callbacks.touch_end(point.timestamp);
    (void)system.frames.step(runtime::Clock::now()+120ms);
    check(take(system.scroll.snapshot()).scroll_offset>before_release,"native touch callbacks did not deliver inertial scrolling");

    gui::detail::AndroidPointerState android;gui::detail::AndroidPointerFrame frame;frame.device=3;
    const auto dispatch_android=[&](const gui::detail::AndroidPointerFrame& current){
        using A=gui::detail::AndroidPointerAction;
        if(current.action==A::down||current.action==A::secondary_down)callbacks.touch_begin(current.time);
        auto result=android.translate(current);for(auto& input:result.events)callbacks.input(std::move(input));
        if(current.action==A::cancel||current.canceled)callbacks.touch_cancel();
        else if(current.action==A::up)callbacks.touch_end(current.time);
    };
    using A=gui::detail::AndroidPointerAction;using T=gui::detail::AndroidPointerTool;
    frame.action=A::down;frame.time=20.;frame.samples={{.id=1,.tool=T::touch,.x=50,.y=80}};dispatch_android(frame);
    frame.action=A::secondary_down;frame.index=1;frame.time=20.001;frame.samples.push_back({.id=2,.tool=T::touch,.x=70,.y=80});dispatch_android(frame);
    frame.action=A::move;frame.time=20.02;frame.samples[0].y=frame.samples[1].y=60;dispatch_android(frame);
    frame.time=20.04;frame.samples[0].y=frame.samples[1].y=40;dispatch_android(frame);
    const auto android_release=take(system.scroll.snapshot()).scroll_offset;
    frame.action=A::secondary_up;frame.index=1;frame.time=20.041;dispatch_android(frame);
    frame.action=A::up;frame.index=0;frame.time=20.042;frame.samples.resize(1);dispatch_android(frame);
    (void)system.frames.step(runtime::Clock::now()+200ms);
    check(take(system.scroll.snapshot()).scroll_offset>android_release,"Android touch callbacks did not deliver inertial scrolling");
}
