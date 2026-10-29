#include <iiGUI/HostCommon.h>
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/MountTransaction.h>
#include <iiGUI/WebSurface.h>
#include <iiGUI/WebAsync.h>
#include <iiGUI/DesktopCommon.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include <iiGUI/HostInputQueue.h>
#include <cstring>

namespace ii::gui {
namespace {
EM_JS(int,web_requirements,(),{return typeof OffscreenCanvas!=='undefined'&&typeof Atomics.waitAsync==='function'&&typeof self.requestAnimationFrame==='function';});
// Keep one animation-frame request across Core wakes. Repeated UI work must
// not cancel every frame or starve browser tasks with resolved-promise loops.
IIGUI_WEB_ASYNC_JS(double,web_wait,(void* signal,int expected,int timeout,int frame),{
    let state=Module['iiGUIWait'];
    if(!state){
        const channel=new MessageChannel();state={signal,request:null,ready:-1,active:true,channel,resume:null};
        channel.port1.onmessage=()=>{const resume=state.resume;state.resume=null;if(resume)resume();};Module['iiGUIWait']=state;
    }
    const index=signal>>2;
    if(frame){if(state.request===null&&state.ready<0)state.request=requestAnimationFrame(time=>{
        state.request=null;if(!state.active)return;state.ready=time;Atomics.add(HEAP32,index,1);Atomics.notify(HEAP32,index);
    });}
    else{if(state.request!==null)cancelAnimationFrame(state.request);state.request=null;state.ready=-1;}
    const take=()=>{const ready=state.ready;state.ready=-1;return ready;};
    if(state.ready>=0)return take();
    if(Atomics.load(HEAP32,index)!==expected){
        await new Promise(resolve=>{state.resume=resolve;state.channel.port2.postMessage(0);});return take();
    }
    const wait=Atomics.waitAsync(HEAP32,index,expected,timeout<0?Infinity:Math.max(1,timeout));
    if(wait.async)await wait.value;
    else await new Promise(resolve=>{state.resume=resolve;state.channel.port2.postMessage(0);});
    return take();
});
EM_JS(void,web_wait_close,(),{
    const state=Module['iiGUIWait'];if(!state)return;state.active=false;
    if(state.request!==null)cancelAnimationFrame(state.request);state.channel.port1.close();state.channel.port2.close();delete Module['iiGUIWait'];
});
WidgetKey key(std::string_view value){if(value=="Tab")return WidgetKey::tab;if(value=="Enter")return WidgetKey::enter;if(value=="Escape")return WidgetKey::escape;if(value==" ")return WidgetKey::space;if(value=="ArrowLeft")return WidgetKey::left;if(value=="ArrowRight")return WidgetKey::right;if(value=="ArrowUp")return WidgetKey::up;if(value=="ArrowDown")return WidgetKey::down;if(value=="Home")return WidgetKey::home;if(value=="End")return WidgetKey::end;if(value=="Backspace")return WidgetKey::backspace;if(value=="Delete")return WidgetKey::delete_forward;if(value=="PageUp")return WidgetKey::page_up;if(value=="PageDown")return WidgetKey::page_down;return WidgetKey::character;}
}
namespace detail {void web_wake(std::atomic<std::int32_t>& signal)noexcept{signal.fetch_add(1,std::memory_order_release);EM_ASM({Atomics.notify(HEAP32,$0>>2);},&signal);}}
struct Host::Impl final {
    enum class Phase{created,running,closed};
    HostOptions options;std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    // Wake storage stays alive until HostLoop disconnect has barred workers.
    std::shared_ptr<std::atomic<std::int32_t>> wake=std::make_shared<std::atomic<std::int32_t>>(0);
    const std::thread::id owner=std::this_thread::get_id();
    detail::WebSurfaceParent surface;HostCallbacks callbacks;WindowSnapshot state;HostStats statistics;
    Phase phase=Phase::created;std::optional<core::Error> failure;detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> preferences=std::make_shared<MotionPreferences>();
    detail::HostInputQueue inputs;bool resize_pending=false,visibility_pending=false,hidden=false,lost=false,lost_pending=false,restore_pending=false,mounting=false,opened=false,close_pending=false;
    std::size_t motion_clients=0;double last_frame=0;
    explicit Impl(HostOptions value):options(detail::validate_options(std::move(value))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){
        if(emscripten_is_main_browser_thread())throw std::invalid_argument("Web Host requires the Core application pthread (-sPROXY_TO_PTHREAD=1)");
        surface.size=options.size;surface.wake=wake.get();state.title=options.title;state.size=options.size;
    }
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"Web GUI requires the application pthread");if(phase!=Phase::running||close_pending)return core::fail(core::ErrorCode::closed,"Web host is closed");return {};}
    template<class F>void guard(F&& work)noexcept{try{work();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Web host callback failed",std::current_exception()};defer_close();}}
    void signal(){detail::web_wake(*wake);}
    void defer_close()noexcept{close_pending=true;dispatch->retire();dispatch->loop.request_stop();signal();}
    void close()noexcept{if(phase!=Phase::running)return;phase=Phase::closed;dispatch->retire();canvas.clear();if(widgets){widgets->close();widgets.reset();}state.visible=false;state.closed=true;presentations->close();surface.visible=false;dispatch->loop.request_stop();if(callbacks.closed)try{callbacks.closed();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Web close callback failed",std::current_exception()};}signal();}
    void push(WidgetInput value)noexcept{
        try{if(phase!=Phase::running)return;if(!inputs.push(std::move(value)))throw std::runtime_error("Browser input queue is full");}
        catch(...){if(!failure)failure=core::Error{core::ErrorCode::full,"Browser input admission failed",std::current_exception()};}
        signal();
    }
    void resize(){double dimensions[3]{};MAIN_THREAD_EM_ASM({const canvas=document.querySelector('#canvas');const r=canvas.getBoundingClientRect();HEAPF64[$0>>3]=r.width;HEAPF64[($0>>3)+1]=r.height;HEAPF64[($0>>3)+2]=window.devicePixelRatio||1;},dimensions);
        const Size size{dimensions[0],dimensions[1]};if(!detail::valid_size(size)||!std::isfinite(dimensions[2])||dimensions[2]<=0||dimensions[2]>8)throw std::runtime_error("Browser canvas exceeds size/DPI budget");const bool changed=state.size!=size||state.backing_scale!=dimensions[2];surface.size=state.size=size;surface.scale=state.backing_scale=dimensions[2];
        if(changed){canvas.dirty=true;if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)widgets->native_event(&event);}if(opened&&callbacks.resized)callbacks.resized(size);}}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    detail::WidgetFrameHooks hooks(){return {.frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},.activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;signal();},.preferences=preferences,.renderer=options.renderer,.events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void drain(double frame_time){guard([&]{
        if(failure||close_pending){defer_close();return;}
        if(resize_pending){resize_pending=false;resize();}
        if(visibility_pending){visibility_pending=false;surface.visible=state.visible=!hidden;if(!hidden){last_frame=0;if(frames())frames()->reset_clock();}if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)widgets->native_event(&event);}}
        if(restore_pending){restore_pending=false;lost=false;if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::restore};(void)widgets->native_event(&event);}if(canvas.frame){auto frame=*canvas.frame;auto result=canvas.set(std::move(frame),&surface,nullptr,surface.scale,presentations);if(!result)throw std::runtime_error(result.error().message);}}
        if(lost_pending){lost_pending=false;canvas.native.reset();if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::lost};(void)widgets->native_event(&event);}}
        while(!inputs.empty()&&phase==Phase::running&&!close_pending){auto input=*inputs.pop();if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::input,&input};(void)widgets->native_event(&event);}}
        if(phase!=Phase::running||close_pending)return;++statistics.dispatches;dispatch->loop.poll(options.budget);if(dispatch->loop.state()==runtime::LoopState::stopped){defer_close();return;}
        if(frame_time>=0&&!hidden&&!lost){++statistics.display_callbacks;statistics.display_synced=true;if(last_frame>0)statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double,std::milli>(frame_time-last_frame));last_frame=frame_time;if(frames())(void)frames()->step();}
        if(surface.visible&&!lost&&phase==Phase::running&&!close_pending){if(widgets)widgets->present();canvas.flush(surface.scale,surface.size.height);}
    });if(close_pending)close();}
    static EM_BOOL mouse(int kind,const EmscriptenMouseEvent* e,void* raw){auto& self=*static_cast<Impl*>(raw);WidgetInput input;input.kind=kind==EMSCRIPTEN_EVENT_MOUSEDOWN?WidgetInputKind::pointer_down:kind==EMSCRIPTEN_EVENT_MOUSEUP?WidgetInputKind::pointer_up:kind==EMSCRIPTEN_EVENT_MOUSELEAVE?WidgetInputKind::pointer_leave:WidgetInputKind::pointer_move;input.x=e->targetX;input.y=e->targetY;input.buttons=e->buttons;input.shift=e->shiftKey;input.control=e->ctrlKey;input.alt=e->altKey;input.meta=e->metaKey;input.timestamp=e->timestamp/1000.;self.push(std::move(input));return EM_FALSE;}
    static EM_BOOL keyboard(int kind,const EmscriptenKeyboardEvent* e,void* raw){auto& self=*static_cast<Impl*>(raw);WidgetInput input;input.kind=kind==EMSCRIPTEN_EVENT_KEYDOWN?WidgetInputKind::key_down:WidgetInputKind::key_up;input.key=key(e->key);input.shift=e->shiftKey;input.control=e->ctrlKey;input.alt=e->altKey;input.meta=e->metaKey;input.timestamp=e->timestamp/1000.;if(input.key==WidgetKey::character&&!input.control&&!input.alt&&!input.meta&&std::strlen(e->key)<=4)input.text=e->key;self.push(std::move(input));return EM_FALSE;}
    void callbacks_install(){
        emscripten_set_mousedown_callback("#canvas",this,EM_FALSE,&mouse);emscripten_set_mouseup_callback("#canvas",this,EM_FALSE,&mouse);emscripten_set_mousemove_callback("#canvas",this,EM_FALSE,&mouse);emscripten_set_mouseleave_callback("#canvas",this,EM_FALSE,&mouse);
        emscripten_set_keydown_callback("#canvas",this,EM_FALSE,&keyboard);emscripten_set_keyup_callback("#canvas",this,EM_FALSE,&keyboard);
        emscripten_set_wheel_callback("#canvas",this,EM_FALSE,+[](int,const EmscriptenWheelEvent* e,void* raw)->EM_BOOL{WidgetInput input;input.kind=WidgetInputKind::scroll;input.x=e->mouse.targetX;input.y=e->mouse.targetY;const auto multiplier=e->deltaMode==DOM_DELTA_PIXEL?1.:e->deltaMode==DOM_DELTA_LINE?20.:200.;input.scroll_x=-e->deltaX*multiplier;input.scroll_y=-e->deltaY*multiplier;input.precise_scroll=e->deltaMode==DOM_DELTA_PIXEL;static_cast<Impl*>(raw)->push(std::move(input));return EM_FALSE;});
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,this,EM_FALSE,+[](int,const EmscriptenUiEvent*,void* raw)->EM_BOOL{auto& self=*static_cast<Impl*>(raw);self.resize_pending=true;self.signal();return EM_FALSE;});
        emscripten_set_blur_callback("#canvas",this,EM_FALSE,+[](int,const EmscriptenFocusEvent*,void* raw)->EM_BOOL{static_cast<Impl*>(raw)->push({.kind=WidgetInputKind::focus_lost});return EM_FALSE;});
        emscripten_set_visibilitychange_callback(this,EM_FALSE,+[](int,const EmscriptenVisibilityChangeEvent* e,void* raw)->EM_BOOL{auto& self=*static_cast<Impl*>(raw);self.hidden=e->hidden;self.visibility_pending=true;self.signal();return EM_FALSE;});
        emscripten_set_webglcontextlost_callback("#canvas",this,EM_FALSE,+[](int,const void*,void* raw)->EM_BOOL{auto& self=*static_cast<Impl*>(raw);self.lost=true;self.lost_pending=true;self.signal();return EM_TRUE;});
        emscripten_set_webglcontextrestored_callback("#canvas",this,EM_FALSE,+[](int,const void*,void* raw)->EM_BOOL{auto& self=*static_cast<Impl*>(raw);self.restore_pending=true;self.signal();return EM_TRUE;});
    }
    void callbacks_remove(){emscripten_set_mousedown_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mouseup_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mousemove_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mouseleave_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_keydown_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_keyup_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_wheel_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_FALSE,nullptr);emscripten_set_blur_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_visibilitychange_callback(nullptr,EM_FALSE,nullptr);emscripten_set_webglcontextlost_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_webglcontextrestored_callback("#canvas",nullptr,EM_FALSE,nullptr);}
};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Host::~Host(){if(!is_ui_thread()||impl_->phase==Impl::Phase::running)std::terminate();impl_->presentations->close();impl_->dispatch->retire();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::run(HostCallbacks callbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Web Host requires its application pthread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Host is single use");if(!web_requirements())return core::fail(core::ErrorCode::unsupported,"Browser requires OffscreenCanvas, worker requestAnimationFrame and Atomics.waitAsync");
    impl_->callbacks=std::move(callbacks);impl_->phase=Impl::Phase::running;
    try{MAIN_THREAD_EM_ASM({const c=document.querySelector('#canvas');if(!c)throw new Error('iiGUI requires #canvas');c.tabIndex=0;c.style.touchAction='none';document.title=UTF8ToString($0);},impl_->state.title.c_str());
        impl_->resize();impl_->state.visible=true;impl_->dispatch->connect([wake=impl_->wake]()noexcept{detail::web_wake(*wake);},{.monitor_io=false});impl_->callbacks_install();impl_->opened=true;if(impl_->callbacks.opened)impl_->callbacks.opened();double frame=-1;
        while(impl_->phase==Impl::Phase::running){const auto expected=impl_->wake->load(std::memory_order_acquire);impl_->drain(frame);if(impl_->phase!=Impl::Phase::running)break;const auto timeout=detail::deadline_timeout(impl_->dispatch->loop.next_deadline());frame=web_wait(impl_->wake.get(),expected,timeout,!impl_->hidden&&!impl_->lost&&(impl_->motion_clients||impl_->options.frame_loop));}
    }catch(...){if(!impl_->failure)impl_->failure=core::Error{core::ErrorCode::failed,"Cannot run Web host",std::current_exception()};}
    impl_->close();web_wait_close();impl_->callbacks_remove();(void)impl_->dispatch->disconnect();if(impl_->failure)return std::unexpected(*impl_->failure);return {};
}
core::Result<void> Host::show(Host&,HostCallbacks){return core::fail(core::ErrorCode::unsupported,"Browser host owns one canvas; use iiGUI popovers");}
core::Result<void> Host::choose_file(FileDialogOptions,FileDialogCompletion){return core::fail(core::ErrorCode::unsupported,"Browser file handles require a separate FileProvider adapter");}
core::Result<void> Host::window_action(WindowAction){return core::fail(core::ErrorCode::unsupported,"Browser owns top-level window geometry");}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> menu){if(!detail::valid_menu(menu))return core::fail(core::ErrorCode::invalid_argument,"Invalid menu");return core::fail(core::ErrorCode::unsupported,"Use iiGUI menus within the canvas");}
core::Result<void> Host::set_title(std::string_view title){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(title))return core::fail(core::ErrorCode::invalid_argument,"Invalid title");impl_->state.title=title;MAIN_THREAD_EM_ASM({document.title=UTF8ToString($0);},impl_->state.title.c_str());return {};}
core::Result<void> Host::set_text(std::string_view text){if(!detail::valid_title(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid text");auto mounted=mount_widgets([text=std::string(text)](WidgetBuilder& widgets)->core::Result<void>{auto r=widgets.label({},"host-text",{.text=text});if(!r)return std::unexpected(r.error());return {};});if(!mounted)return std::unexpected(mounted.error());impl_->state.text=text;return {};}
core::Result<void> Host::set_content_size(Size size){return set_window_geometry({.width=size.width,.height=size.height});}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){if(auto r=impl_->ready();!r)return r;auto next=resolve_window_geometry({impl_->state.title,impl_->state.size,impl_->options.minimum_size},update);if(!next)return std::unexpected(next.error());impl_->options.minimum_size=next->minimum_size;if(update.title){auto r=set_title(*update.title);if(!r)return r;}if(update.width||update.height){MAIN_THREAD_EM_ASM({const canvas=document.querySelector('#canvas');canvas.style.width=$0+'px';canvas.style.height=$1+'px';},next->size.width,next->size.height);impl_->resize_pending=true;impl_->signal();}return {};}
core::Result<void> Host::request_close(){if(auto r=impl_->ready();!r)return r;impl_->guard([&]{if(!impl_->callbacks.close_requested||impl_->callbacks.close_requested())impl_->defer_close();});return {};}
core::Result<void> Host::set_canvas(CanvasFrame frame){if(auto r=impl_->ready();!r)return r;if(valid_canvas(frame)&&!detail::full_surface_canvas(frame,impl_->state.size))return core::fail(core::ErrorCode::unsupported,"This host requires a full-window raw canvas; compose subregions with C++ widgets");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;auto result=impl_->canvas.set(std::move(frame),&impl_->surface,nullptr,impl_->surface.scale,impl_->presentations);if(result){if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}impl_->signal();}return result;}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Empty widget factory");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");struct Reset{bool& value;~Reset(){value=false;}}reset{impl_->mounting};impl_->mounting=true;
    detail::MountTransaction transaction([this]()noexcept{if(impl_->phase!=Impl::Phase::running)return;impl_->guard([&]{if(impl_->widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)impl_->widgets->native_event(&event);impl_->widgets->present();}else if(impl_->canvas.frame){impl_->canvas.dirty=true;impl_->canvas.flush(impl_->surface.scale,impl_->surface.size.height);}});});
    try{auto hooks=impl_->hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});},appearance,std::move(hooks));if(auto r=replacement->construct(std::move(factory));!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(auto r=replacement->attach(&impl_->surface);!r)return std::unexpected(r.error());if(auto r=replacement->commit_window_properties();!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());impl_->canvas.clear();if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);transaction.commit();auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());return WidgetMount(published);}catch(...){return core::fail(core::ErrorCode::failed,"Web widget mount failed",std::current_exception());}}
core::Result<void> Host::unmount_widgets(){if(auto r=impl_->ready();!r)return r;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Widget generation retired");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::clear_canvas(){if(auto r=impl_->ready();!r)return r;impl_->canvas.clear();return {};}
core::Result<GraphicsInfo> Host::graphics_info()const{if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->graphics_info():core::Result<GraphicsInfo>{impl_->canvas.info()};}
core::Result<PixelBuffer> Host::capture_canvas(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->capture():impl_->canvas.capture(impl_->surface.scale,impl_->surface.size.height);}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return std::ref(impl_->dispatch->loop);}
core::Result<WindowSnapshot> Host::snapshot()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Snapshot requires host owner");if(impl_->phase==Impl::Phase::created)return core::fail(core::ErrorCode::closed,"Host not started");return impl_->state;}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Stats require host owner");auto result=impl_->statistics;result.loop=impl_->dispatch->loop.stats();result.host=impl_->dispatch->host_stats();return result;}
}
