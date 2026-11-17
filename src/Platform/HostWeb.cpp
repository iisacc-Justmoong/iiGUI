#include <iiGUI/HostCommon.h>
#include <iiGUI/HostActivity.h>
#include "HostFailure.h"
#include "HostCallback.h"
#include <iiGUI/DispatchState.h>
#include <iiGUI/CanvasState.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/MountTransaction.h>
#include <iiGUI/WebSurface.h>
#include <iiGUI/WebSurfaceSize.h>
#include <iiGUI/WebAsync.h>
#include <iiGUI/DesktopCommon.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/threading.h>
#include "WebHostEvents.h"
#include <cstring>

namespace ii::gui {
namespace {
#if defined(__EMSCRIPTEN_PTHREADS__)
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
#else
EM_JS(int,web_requirements,(),{return typeof document!=='undefined'&&typeof self.requestAnimationFrame==='function';});
IIGUI_WEB_ASYNC_JS(double,web_wait,(void* signal,int expected,int timeout,int frame),{
    /* IIGUI_SINGLE_WAIT_BEGIN */
    let state=Module['iiGUIWait'];
    if(!state){
        const channel=new MessageChannel();
        state={signal,request:null,ready:-1,active:true,channel,resume:null,timer:null,sequence:0,deferred:null};
        state.wake=()=>{const resume=state.resume;state.resume=null;state.deferred=null;if(state.timer!==null){clearTimeout(state.timer);state.timer=null;}if(resume)resume();};
        channel.port1.onmessage=event=>{if(state.deferred!==null&&state.deferred===event.data)state.wake();};Module['iiGUIWait']=state;
    }
    const index=signal>>2;
    if(frame){if(state.request===null&&state.ready<0)state.request=requestAnimationFrame(time=>{
        state.request=null;if(!state.active)return;state.ready=time;++HEAP32[index];state.wake();
    });}
    else{if(state.request!==null)cancelAnimationFrame(state.request);state.request=null;state.ready=-1;}
    const take=()=>{const ready=state.ready;state.ready=-1;return ready;};
    if(state.ready>=0)return take();
    if(HEAP32[index]!==expected){
        await new Promise(resolve=>{state.resume=resolve;state.deferred=++state.sequence;state.channel.port2.postMessage(state.deferred);});return take();
    }
    await new Promise(resolve=>{
        state.resume=resolve;
        if(timeout>=0)state.timer=setTimeout(()=>state.wake(),Math.max(1,timeout));
    });
    return take();
    /* IIGUI_SINGLE_WAIT_END */
});
#endif
EM_JS(void,web_wait_close,(),{
    const state=Module['iiGUIWait'];if(!state)return;state.active=false;
    if(state.request!==null)cancelAnimationFrame(state.request);if(state.timer!=null)clearTimeout(state.timer);state.channel.port1.close();state.channel.port2.close();delete Module['iiGUIWait'];
});
WidgetKey key(std::string_view value){if(value=="Tab")return WidgetKey::tab;if(value=="Enter")return WidgetKey::enter;if(value=="Escape")return WidgetKey::escape;if(value==" ")return WidgetKey::space;if(value=="ArrowLeft")return WidgetKey::left;if(value=="ArrowRight")return WidgetKey::right;if(value=="ArrowUp")return WidgetKey::up;if(value=="ArrowDown")return WidgetKey::down;if(value=="Home")return WidgetKey::home;if(value=="End")return WidgetKey::end;if(value=="Backspace")return WidgetKey::backspace;if(value=="Delete")return WidgetKey::delete_forward;if(value=="PageUp")return WidgetKey::page_up;if(value=="PageDown")return WidgetKey::page_down;return WidgetKey::character;}
}
namespace detail {void web_wake(std::atomic<std::int32_t>& signal)noexcept{
    signal.fetch_add(1,std::memory_order_release);
#if defined(__EMSCRIPTEN_PTHREADS__)
    EM_ASM({Atomics.notify(HEAP32,$0>>2);},&signal);
#else
    EM_ASM({/* IIGUI_SINGLE_WAKE_BEGIN */const state=Module['iiGUIWait'];if(state&&state.wake)state.wake();/* IIGUI_SINGLE_WAKE_END */});
#endif
}}
extern "C" EMSCRIPTEN_KEEPALIVE void iigui_web_accessibility_event(void* raw,const char* id,std::uint32_t generation,int action){
    auto* parent=static_cast<detail::WebSurfaceParent*>(raw);
    if(!parent||!id||!generation||generation!=parent->accessibility_active.load(std::memory_order_acquire)||action<0||action>8)return;
    try {
        auto input=detail::web_accessibility_input(id,action);if(!input)return;
        if(parent->accessibility_post)parent->accessibility_post(std::move(*input),generation);
    }catch(...){parent->accessibility_overflow.store(true,std::memory_order_release);if(parent->wake)detail::web_wake(*parent->wake);}
}
extern "C" EMSCRIPTEN_KEEPALIVE void iigui_web_text_event(void* raw,std::uint32_t generation,std::uint32_t sequence,
        int action,const char* id,const char* text,const char* before,std::uint32_t start,std::uint32_t length,
        std::uint32_t selection,std::uint32_t selection_length,int flags){
    auto* parent=static_cast<detail::WebSurfaceParent*>(raw);
    if(!parent||!id||!text||!before||!generation||!sequence||
       generation!=parent->text_active.load(std::memory_order_acquire)||action<0||action>9||(flags&~15))return;
    try {
        const auto bounded=[](const char* value){return strnlen(value,detail::web_text_limit+1);};
        const auto id_length=bounded(id),text_length=bounded(text),before_length=bounded(before);
        if(!id_length||id_length>detail::web_text_limit||text_length>detail::web_text_limit||before_length>detail::web_text_limit)return;
        detail::WebTextMessage message{.action=static_cast<detail::WebTextAction>(action),
            .id=std::string(id,id_length),.text=std::string(text,text_length),.before=std::string(before,before_length),
            .generation=generation,.sequence=sequence,.start=start,.length=length,
            .selection_start=selection,.selection_length=selection_length,.has_before=bool(flags&1),
            .first_composition=bool(flags&2),.selection_reversed=bool(flags&4),.shift=bool(flags&8)};
        if(parent->text_post)parent->text_post(std::move(message));
    }catch(...){parent->text_overflow.store(true,std::memory_order_release);if(parent->wake)detail::web_wake(*parent->wake);}
}
struct Host::Impl final {
    enum class Phase{created,running,closed};
    HostOptions options;std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    // Wake storage stays alive until HostLoop disconnect has barred workers.
    std::shared_ptr<std::atomic<std::int32_t>> wake=std::make_shared<std::atomic<std::int32_t>>(0);
    const std::thread::id owner=std::this_thread::get_id();
    detail::WebSurfaceParent surface;HostCallbacks callbacks;detail::HostActivity activity,foreground;WindowSnapshot state;HostStats statistics;
    Phase phase=Phase::created;detail::HostFailure failure;detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> preferences=std::make_shared<MotionPreferences>();
    detail::WebHostEvents events;detail::WebSurfaceVisibility surface_visibility;bool resize_pending=false,hidden=false,lost=false,mounting=false,opened=false,close_pending=false;
    std::size_t motion_clients=0;double last_frame=0;
    explicit Impl(HostOptions value):options(detail::validate_options(std::move(value))),dispatch(std::make_shared<detail::DispatchState>(options.task_capacity,options.event_loop)){
#if defined(__EMSCRIPTEN_PTHREADS__)
        if(emscripten_is_main_browser_thread())throw std::invalid_argument("Web Host requires the Core application pthread (-sPROXY_TO_PTHREAD=1)");
#else
        if(!emscripten_is_main_browser_thread())throw std::invalid_argument("Single-thread Web Host requires the browser main thread");
#endif
        surface.size=options.size;surface.wake=wake.get();
        surface.accessibility_post=[this](WidgetInput input,std::uint32_t generation){post({.kind=detail::WebHostEventKind::input,.input=std::move(input),.accessibility_generation=generation});};
        surface.text_post=[this](detail::WebTextMessage message){post({.kind=detail::WebHostEventKind::text_input,.text=std::move(message)});};
        state.title=options.title;state.size=options.size;
    }
    bool on_owner()const{return owner==std::this_thread::get_id();}
    core::Result<void> ready()const{if(!on_owner())return core::fail(core::ErrorCode::wrong_executor,"Web GUI requires its owner thread");if(phase!=Phase::running||close_pending)return core::fail(core::ErrorCode::closed,"Web host is closed");return {};}
    template<class F>void guard(F&& work)noexcept{try{work();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Web host callback failed",std::current_exception()};defer_close();}}
    void signal(){detail::web_wake(*wake);}
    void defer_close()noexcept{close_pending=true;dispatch->retire();dispatch->loop.request_stop();signal();}
    void close()noexcept{if(phase!=Phase::running)return;phase=Phase::closed;activity.close();foreground.close();dispatch->retire();canvas.clear();if(widgets){widgets->close();widgets.reset();}state.visible=false;state.closed=true;presentations->close();surface.visible=false;dispatch->loop.request_stop();if(callbacks.closed)try{callbacks.closed();}catch(...){if(!failure)failure=core::Error{core::ErrorCode::failed,"Web close callback failed",std::current_exception()};}signal();}
    void install_minimum(Size size){MAIN_THREAD_EM_ASM({
        /* IIGUI_CANVAS_MINIMUM_INSTALL_BEGIN */
        (function() {
        const previous=Module['iiGUICanvasMinimum'];if(previous)previous.dispose();delete Module['iiGUICanvasMinimum'];
        const element=document.querySelector('#canvas');
        if(element&&element.hasAttribute('data-iigui-respect-minimum')){
            const style=element.style,computed=window.getComputedStyle(element);
            const field=(name,base)=>({name,original:style.getPropertyValue(name),priority:style.getPropertyPriority(name),
                base:!base||base==='auto'||base==='none'?'0px':base,last:null,overridden:false});
            const width=field('min-width',computed.minWidth),height=field('min-height',computed.minHeight);
            const current=(f)=>style.getPropertyValue(f.name)===f.last&&style.getPropertyPriority(f.name)===f.priority;
            const apply=(f,value)=>{
                if(f.overridden)return;
                if(f.last!==null&&!current(f)){f.overridden=true;return;}
                const candidate='max('+f.base+', '+value+'px)';
                if(!window.CSS||!window.CSS.supports(f.name,candidate))return;
                style.setProperty(f.name,candidate,f.priority);f.last=style.getPropertyValue(f.name);
            };
            const restore=(f)=>{if(f.last===null||!current(f))return;
                if(f.original)style.setProperty(f.name,f.original,f.priority);else style.removeProperty(f.name);};
            const state={apply:(w,h)=>{apply(width,w);apply(height,h);},dispose:()=>{restore(width);restore(height);}};
            Module['iiGUICanvasMinimum']=state;state.apply($0,$1);
        }
        })();
        /* IIGUI_CANVAS_MINIMUM_INSTALL_END */
    },size.width,size.height);}
    void update_minimum(Size size){MAIN_THREAD_EM_ASM({
        /* IIGUI_CANVAS_MINIMUM_UPDATE_BEGIN */
        const state=Module['iiGUICanvasMinimum'];if(state)state.apply($0,$1);
        /* IIGUI_CANVAS_MINIMUM_UPDATE_END */
    },size.width,size.height);}
    void remove_minimum(){MAIN_THREAD_EM_ASM({
        /* IIGUI_CANVAS_MINIMUM_REMOVE_BEGIN */
        const state=Module['iiGUICanvasMinimum'];if(state){state.dispose();delete Module['iiGUICanvasMinimum'];}
        /* IIGUI_CANVAS_MINIMUM_REMOVE_END */
    });}
    void post(detail::WebHostEvent event)noexcept{events.push(std::move(event));signal();}
    void push(WidgetInput value)noexcept{post({.kind=detail::WebHostEventKind::input,.input=std::move(value)});}
    void resize(){double dimensions[3]{};MAIN_THREAD_EM_ASM({const canvas=document.querySelector('#canvas');const r=canvas.getBoundingClientRect();HEAPF64[$0>>3]=r.width;HEAPF64[($0>>3)+1]=r.height;HEAPF64[($0>>3)+2]=window.devicePixelRatio||1;},dimensions);
        const Size size{dimensions[0],dimensions[1]};const auto change=surface_visibility.update(size,dimensions[2],state.size,state.backing_scale);const bool was_visible=surface.visible;
        if(change.size==detail::WebSurfaceSize::invalid)throw std::runtime_error("Browser canvas exceeds size/DPI budget");
        surface.visible=state.visible=surface_visibility.visible(hidden);
        if(change.size==detail::WebSurfaceSize::suspended){if(was_visible&&widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)widgets->native_event(&event);}return;}
        surface.size=state.size=size;surface.scale=state.backing_scale=dimensions[2];
        if(change.resumed){last_frame=0;if(frames())frames()->reset_clock();}
        if(change.changed||change.resumed){canvas.dirty=true;if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)widgets->native_event(&event);}if(change.changed&&opened&&callbacks.resized)callbacks.resized(size);}}
    runtime::FrameLoop* frames(){return options.frame_loop?options.frame_loop:motion_frames.get();}
    detail::WidgetFrameHooks hooks(){return {.frames=[this]()->core::Result<runtime::FrameLoop*>{if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);return frames();},.activity=[this](bool active){if(active)++motion_clients;else if(motion_clients)--motion_clients;signal();},.preferences=preferences,.renderer=options.renderer,.events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations};}
    void drain(double frame_time){guard([&]{
        if(failure||close_pending){defer_close();return;}
        if(resize_pending){resize_pending=false;resize();}
        if(events.take_overflow()||surface.accessibility_overflow.exchange(false,std::memory_order_acq_rel)||surface.text_overflow.exchange(false,std::memory_order_acq_rel)){failure=core::Error{core::ErrorCode::full,"Browser event admission failed"};defer_close();return;}
        while(auto browser_event=events.pop()){
            switch(browser_event->kind){
            case detail::WebHostEventKind::resize:resize();break;
            case detail::WebHostEventKind::visibility:
                hidden=browser_event->hidden;foreground.observe(!hidden);surface.visible=state.visible=surface_visibility.visible(hidden);activity.observe(!hidden);
                if(!hidden){last_frame=0;if(frames())frames()->reset_clock();}
                if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)widgets->native_event(&event);}break;
            case detail::WebHostEventKind::context_lost:
                lost=true;canvas.native.reset();if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::lost};(void)widgets->native_event(&event);}break;
            case detail::WebHostEventKind::context_restored:
                lost=false;if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::restore};(void)widgets->native_event(&event);}
                if(canvas.frame){auto frame=*canvas.frame;auto result=canvas.set(std::move(frame),&surface,nullptr,surface.scale,presentations);if(!result)throw std::runtime_error(result.error().message);}break;
            case detail::WebHostEventKind::input:
                if(browser_event->accessibility_generation&&browser_event->accessibility_generation!=surface.accessibility_active.load(std::memory_order_acquire))break;
                if(browser_event->accessibility_generation&&widgets&&browser_event->input.kind!=WidgetInputKind::focus_lost){
                    const auto current=widgets;auto target=current->accessibility_snapshot(browser_event->input.target);
                    if(!target){if(target.error().code==core::ErrorCode::not_found||target.error().code==core::ErrorCode::closed)break;throw std::runtime_error(target.error().message);}
                    if(current!=widgets||!target->attached||!target->visible||target->input_blocked||!target->enabled||
                       (browser_event->input.kind==WidgetInputKind::activate&&!target->accessibility_actionable))break;
                }
                if(widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::input,&browser_event->input};(void)widgets->native_event(&event);}break;
            case detail::WebHostEventKind::text_input:
                if(browser_event->text && browser_event->text->generation==surface.text_active.load(std::memory_order_acquire) && widgets){
                    const detail::WebSurfaceEvent event{.kind=detail::WebSurfaceEventKind::text_input,.text=&*browser_event->text};
                    (void)widgets->native_event(&event);
                }break;
            }
            if(close_pending||phase!=Phase::running)return;
        }
        if(opened)foreground.deliver();
        if(close_pending||phase!=Phase::running)return;
        if(opened)activity.deliver();
        if(close_pending||phase!=Phase::running)return;
        if(phase!=Phase::running||close_pending)return;++statistics.dispatches;dispatch->loop.poll(options.budget);if(dispatch->loop.state()==runtime::LoopState::stopped){defer_close();return;}
        if(frame_time>=0&&surface_visibility.visible(hidden)&&!lost){++statistics.display_callbacks;statistics.display_synced=true;if(last_frame>0)statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double,std::milli>(frame_time-last_frame));last_frame=frame_time;if(frames())(void)frames()->step();}
        if(surface.visible&&!lost&&phase==Phase::running&&!close_pending){if(widgets)widgets->present();canvas.flush(surface.scale,surface.size.height);}
    });if(close_pending)close();}
    static EM_BOOL mouse(int kind,const EmscriptenMouseEvent* e,void* raw){auto& self=*static_cast<Impl*>(raw);WidgetInput input;input.kind=kind==EMSCRIPTEN_EVENT_MOUSEDOWN?WidgetInputKind::pointer_down:kind==EMSCRIPTEN_EVENT_MOUSEUP?WidgetInputKind::pointer_up:kind==EMSCRIPTEN_EVENT_MOUSELEAVE?WidgetInputKind::pointer_leave:WidgetInputKind::pointer_move;input.x=e->targetX;input.y=e->targetY;input.buttons=e->buttons;input.shift=e->shiftKey;input.control=e->ctrlKey;input.alt=e->altKey;input.meta=e->metaKey;input.timestamp=e->timestamp/1000.;self.push(std::move(input));return EM_FALSE;}
    static EM_BOOL keyboard(int kind,const EmscriptenKeyboardEvent* e,void* raw){auto& self=*static_cast<Impl*>(raw);WidgetInput input;input.kind=kind==EMSCRIPTEN_EVENT_KEYDOWN?WidgetInputKind::key_down:WidgetInputKind::key_up;input.key=key(e->key);input.shift=e->shiftKey;input.control=e->ctrlKey;input.alt=e->altKey;input.meta=e->metaKey;input.timestamp=e->timestamp/1000.;if(input.key==WidgetKey::character&&!input.control&&!input.alt&&!input.meta&&std::strlen(e->key)<=4)input.text=e->key;self.push(std::move(input));return EM_FALSE;}
    void callbacks_install(){
        // Synchronous callbacks proxied to the application pthread can deadlock
        // against its synchronous browser calls. Only enqueue on the browser
        // thread; the host drains and delivers the events on its owner thread.
        const auto browser_thread=EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD;
        emscripten_set_mousedown_callback_on_thread("#canvas",this,EM_FALSE,&mouse,browser_thread);emscripten_set_mouseup_callback_on_thread("#canvas",this,EM_FALSE,&mouse,browser_thread);emscripten_set_mousemove_callback_on_thread("#canvas",this,EM_FALSE,&mouse,browser_thread);emscripten_set_mouseleave_callback_on_thread("#canvas",this,EM_FALSE,&mouse,browser_thread);
        emscripten_set_keydown_callback_on_thread("#canvas",this,EM_FALSE,&keyboard,browser_thread);emscripten_set_keyup_callback_on_thread("#canvas",this,EM_FALSE,&keyboard,browser_thread);
        emscripten_set_wheel_callback_on_thread("#canvas",this,EM_FALSE,+[](int,const EmscriptenWheelEvent* e,void* raw)->EM_BOOL{WidgetInput input;input.kind=WidgetInputKind::scroll;input.x=e->mouse.targetX;input.y=e->mouse.targetY;const auto multiplier=e->deltaMode==DOM_DELTA_PIXEL?1.:e->deltaMode==DOM_DELTA_LINE?20.:200.;input.scroll_x=-e->deltaX*multiplier;input.scroll_y=-e->deltaY*multiplier;input.precise_scroll=e->deltaMode==DOM_DELTA_PIXEL;static_cast<Impl*>(raw)->push(std::move(input));return EM_FALSE;},browser_thread);
        emscripten_set_resize_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW,this,EM_FALSE,+[](int,const EmscriptenUiEvent*,void* raw)->EM_BOOL{static_cast<Impl*>(raw)->post({.kind=detail::WebHostEventKind::resize});return EM_FALSE;},browser_thread);
        // A canvas embedded in a resizable page can change size without a
        // window resize. Deliver it through the existing owner-thread queue.
        MAIN_THREAD_EM_ASM({ (function() {
            /* IIGUI_CANVAS_WATCH_BEGIN */
            const previous = Module['iiGUICanvasWatch'];
            if (previous) previous.dispose();
            const state = {active: true, observer: null};
            state.dispose = () => { state.active = false;if (state.observer) state.observer.disconnect(); };
            if (typeof window.ResizeObserver === 'function') {
                state.observer = new window.ResizeObserver(() => {
                    if (state.active) window.dispatchEvent(new Event('resize'));
                });
                state.observer.observe(document.querySelector('#canvas'));
            }
            Module['iiGUICanvasWatch'] = state;
            /* IIGUI_CANVAS_WATCH_END */
        })(); });
        // A browser can change devicePixelRatio when a window moves displays
        // without changing its CSS size. Watch the current ratio, then re-arm.
        MAIN_THREAD_EM_ASM({ (function() {
            /* IIGUI_DPI_WATCH_BEGIN */
            const previous = Module['iiGUIDpiWatch'];
            if (previous) previous.dispose();
            const state = {active: true, query: null, listener: null};
            state.remove = () => {
                if (!state.query || !state.listener) return;
                if (state.query.removeEventListener) state.query.removeEventListener('change', state.listener);
                else if (state.query.removeListener) state.query.removeListener(state.listener);
                state.query = null;state.listener = null;
            };
            state.dispose = () => { state.active = false;state.remove(); };
            state.arm = () => {
                state.remove();
                if (!state.active || typeof window.matchMedia !== 'function') return;
                const query = window.matchMedia('(resolution: ' + (window.devicePixelRatio || 1) + 'dppx)');
                const listener = () => {
                    if (!state.active || state.query !== query) return;
                    state.arm();window.dispatchEvent(new Event('resize'));
                };
                state.query = query;state.listener = listener;
                if (query.addEventListener) query.addEventListener('change', listener);
                else if (query.addListener) query.addListener(listener);
            };
            state.arm();Module['iiGUIDpiWatch'] = state;
            /* IIGUI_DPI_WATCH_END */
        })(); });
        emscripten_set_blur_callback_on_thread("#canvas",this,EM_FALSE,+[](int,const EmscriptenFocusEvent*,void* raw)->EM_BOOL{
            const auto transfer=EM_ASM_INT({const s=Module['iiGUITextInput'];return s&&s.transfer?1:0;});
            if(!transfer)static_cast<Impl*>(raw)->push({.kind=WidgetInputKind::focus_lost});return EM_FALSE;},browser_thread);
        emscripten_set_visibilitychange_callback_on_thread(this,EM_FALSE,+[](int,const EmscriptenVisibilityChangeEvent* e,void* raw)->EM_BOOL{static_cast<Impl*>(raw)->post({.kind=detail::WebHostEventKind::visibility,.hidden=bool(e->hidden)});return EM_FALSE;},browser_thread);
        emscripten_set_webglcontextlost_callback_on_thread("#canvas",this,EM_FALSE,+[](int,const void*,void* raw)->EM_BOOL{static_cast<Impl*>(raw)->post({.kind=detail::WebHostEventKind::context_lost});return EM_TRUE;},browser_thread);
        emscripten_set_webglcontextrestored_callback_on_thread("#canvas",this,EM_FALSE,+[](int,const void*,void* raw)->EM_BOOL{static_cast<Impl*>(raw)->post({.kind=detail::WebHostEventKind::context_restored});return EM_TRUE;},browser_thread);
    }
    void callbacks_remove(){
        remove_minimum();
        MAIN_THREAD_EM_ASM({
            /* IIGUI_CANVAS_UNWATCH_BEGIN */
            const canvasWatch = Module['iiGUICanvasWatch'];
            if (canvasWatch) { canvasWatch.dispose();delete Module['iiGUICanvasWatch']; }
            /* IIGUI_CANVAS_UNWATCH_END */
            /* IIGUI_DPI_UNWATCH_BEGIN */
            const state = Module['iiGUIDpiWatch'];
            if (state) { state.dispose();delete Module['iiGUIDpiWatch']; }
            /* IIGUI_DPI_UNWATCH_END */
        });
        emscripten_set_mousedown_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mouseup_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mousemove_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_mouseleave_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_keydown_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_keyup_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_wheel_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_FALSE,nullptr);emscripten_set_blur_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_visibilitychange_callback(nullptr,EM_FALSE,nullptr);emscripten_set_webglcontextlost_callback("#canvas",nullptr,EM_FALSE,nullptr);emscripten_set_webglcontextrestored_callback("#canvas",nullptr,EM_FALSE,nullptr);}
};
core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Host::~Host(){if(!is_ui_thread()||impl_->phase==Impl::Phase::running)std::terminate();impl_->presentations->close();impl_->dispatch->retire();}
UiExecutor Host::executor()const noexcept{return UiExecutor(impl_->dispatch);}
bool Host::is_ui_thread()const noexcept{return impl_->on_owner();}
core::Result<void> Host::set_application_driver(bool enabled){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application driver requires the owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application driver must be configured before run");if(enabled)return core::fail(core::ErrorCode::unsupported,"Web does not support an independent application driver");return {};}
core::Result<void> Host::set_application_activity_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application activity requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application activity must be configured before run");impl_->activity.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_application_foreground_handler(std::function<void(bool)> handler){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Application foreground requires the host owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Application foreground must be configured before run");impl_->foreground.set_handler(std::move(handler));return {};}
core::Result<void> Host::set_failure_handler(std::function<void(const core::Error&)> handler) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host failure handling requires the owner thread");
    if (impl_->phase != Impl::Phase::created) return core::fail(core::ErrorCode::busy, "Host failure handling must be configured before run or show");
    impl_->failure.set_handler(std::move(handler));
    return {};
}
core::Result<void> Host::run(HostCallbacks callbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Web Host requires its owner thread");if(impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Host is single use");
#if defined(__EMSCRIPTEN_PTHREADS__)
    if(!web_requirements())return core::fail(core::ErrorCode::unsupported,"Browser requires OffscreenCanvas, worker requestAnimationFrame and Atomics.waitAsync");
#else
    if(!web_requirements())return core::fail(core::ErrorCode::unsupported,"Browser requires a document and requestAnimationFrame");
#endif
    impl_->callbacks=std::move(callbacks);impl_->phase=Impl::Phase::running;
    try{impl_->surface.accessibility_enabled=MAIN_THREAD_EM_ASM_INT({const c=document.querySelector('#canvas');if(!c)throw new Error('iiGUI requires #canvas');c.tabIndex=0;c.style.touchAction='none';document.title=UTF8ToString($0);return c.hasAttribute('data-iigui-accessibility')?1:0;},impl_->state.title.c_str());
        impl_->install_minimum(impl_->options.minimum_size);impl_->resize();impl_->hidden=MAIN_THREAD_EM_ASM_INT({return document.hidden?1:0;});impl_->surface.visible=impl_->state.visible=impl_->surface_visibility.visible(impl_->hidden);impl_->activity.observe(!impl_->hidden);impl_->foreground.observe(!impl_->hidden);impl_->dispatch->connect([wake=impl_->wake]()noexcept{detail::web_wake(*wake);},{.monitor_io=false});impl_->callbacks_install();impl_->opened=true;detail::deliver_host_opened(impl_->callbacks.opened);if(impl_->phase==Impl::Phase::running&&!impl_->close_pending){impl_->foreground.open();impl_->activity.open();}double frame=-1;
        while(impl_->phase==Impl::Phase::running){const auto expected=impl_->wake->load(std::memory_order_acquire);impl_->drain(frame);if(impl_->phase!=Impl::Phase::running)break;const auto timeout=detail::deadline_timeout(impl_->dispatch->loop.next_deadline());frame=web_wait(impl_->wake.get(),expected,timeout,impl_->surface_visibility.visible(impl_->hidden)&&!impl_->lost&&(impl_->motion_clients||impl_->options.frame_loop));}
    }catch(...){if(!impl_->failure)impl_->failure=core::Error{core::ErrorCode::failed,"Cannot run Web host",std::current_exception()};}
    impl_->close();web_wait_close();impl_->callbacks_remove();(void)impl_->dispatch->disconnect();if(impl_->failure)return std::unexpected(*impl_->failure);return {};
}
core::Result<void> Host::show_external(HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External event-loop hosting is not implemented on Web");}
core::Result<void> Host::attach_external(void*,HostCallbacks){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on Web");}
core::Result<void> Host::external_parent_resized(){if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"External host requires its owner thread");return core::fail(core::ErrorCode::unsupported,"External surface hosting is not implemented on Web");}
core::Result<void> Host::show(Host&,HostCallbacks){return core::fail(core::ErrorCode::unsupported,"Browser host owns one canvas; use iiGUI popovers");}
core::Result<void> Host::choose_file(FileDialogOptions,FileDialogCompletion){return core::fail(core::ErrorCode::unsupported,"Browser file handles require a separate FileProvider adapter");}
core::Result<void> Host::window_action(WindowAction){return core::fail(core::ErrorCode::unsupported,"Browser owns top-level window geometry");}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> menu){if(!detail::valid_menu(menu))return core::fail(core::ErrorCode::invalid_argument,"Invalid menu");return core::fail(core::ErrorCode::unsupported,"Use iiGUI menus within the canvas");}
core::Result<void> Host::set_title(std::string_view title){if(auto r=impl_->ready();!r)return r;if(!detail::valid_title(title))return core::fail(core::ErrorCode::invalid_argument,"Invalid title");impl_->state.title=title;MAIN_THREAD_EM_ASM({document.title=UTF8ToString($0);},impl_->state.title.c_str());return {};}
core::Result<void> Host::set_text(std::string_view text){if(!detail::valid_title(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid text");auto mounted=mount_widgets([text=std::string(text)](WidgetBuilder& widgets)->core::Result<void>{auto r=widgets.label({},"host-text",{.text=text});if(!r)return std::unexpected(r.error());return {};});if(!mounted)return std::unexpected(mounted.error());impl_->state.text=text;return {};}
core::Result<void> Host::set_content_size(Size size){return set_window_geometry({.width=size.width,.height=size.height});}
core::Result<void> Host::set_window_geometry(const WindowGeometry& update){if(auto r=impl_->ready();!r)return r;auto next=resolve_window_geometry({impl_->state.title,impl_->state.size,impl_->options.minimum_size},update);if(!next)return std::unexpected(next.error());impl_->options.minimum_size=next->minimum_size;if(update.title){auto r=set_title(*update.title);if(!r)return r;}if(update.minimum_width||update.minimum_height)impl_->update_minimum(next->minimum_size);if(update.width||update.height)MAIN_THREAD_EM_ASM({const canvas=document.querySelector('#canvas');canvas.style.width=$0+'px';canvas.style.height=$1+'px';},next->size.width,next->size.height);if(update.width||update.height||update.minimum_width||update.minimum_height){impl_->resize_pending=true;impl_->signal();}return {};}
core::Result<void> Host::request_close(){if(auto r=impl_->ready();!r)return r;impl_->guard([&]{if(!impl_->callbacks.close_requested||impl_->callbacks.close_requested())impl_->defer_close();});return {};}
core::Result<void> Host::set_canvas(CanvasFrame frame){if(auto r=impl_->ready();!r)return r;if(valid_canvas(frame)&&!detail::full_surface_canvas(frame,impl_->state.size))return core::fail(core::ErrorCode::unsupported,"This host requires a full-window raw canvas; compose subregions with C++ widgets");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;auto result=impl_->canvas.set(std::move(frame),&impl_->surface,nullptr,impl_->surface.scale,impl_->presentations);if(result){if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}impl_->signal();}return result;}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory){return mount_widgets(std::move(factory),WidgetAppearance{});}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory,const WidgetAppearance& appearance){if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(!factory)return core::fail(core::ErrorCode::invalid_argument,"Empty widget factory");if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");struct Reset{bool& value;~Reset(){value=false;}}reset{impl_->mounting};impl_->mounting=true;
    detail::MountTransaction transaction([this]()noexcept{if(impl_->phase!=Impl::Phase::running)return;impl_->guard([&]{if(impl_->widgets){const detail::WebSurfaceEvent event{detail::WebSurfaceEventKind::resize};(void)impl_->widgets->native_event(&event);impl_->widgets->present();}else if(impl_->canvas.frame){impl_->canvas.dirty=true;impl_->canvas.flush(impl_->surface.scale,impl_->surface.size.height);}});});
    try{auto hooks=impl_->hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};auto replacement=std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task task){host->guard([&]{if(host->phase==Impl::Phase::running&&!host->close_pending)task();});},appearance,std::move(hooks));if(auto r=replacement->construct(std::move(factory));!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(auto r=replacement->attach(&impl_->surface, detail::make_native_widget_surface);!r)return std::unexpected(r.error());if(auto r=replacement->commit_window_properties();!r)return std::unexpected(r.error());if(auto r=impl_->ready();!r)return std::unexpected(r.error());impl_->canvas.clear();if(impl_->widgets)impl_->widgets->close();impl_->widgets=std::move(replacement);transaction.commit();auto published=impl_->widgets;published->commit_publication();if(auto alive=published->ready();!alive)return std::unexpected(alive.error());const detail::WebSurfaceEvent text_ready{detail::WebSurfaceEventKind::resize};(void)published->native_event(&text_ready);return WidgetMount(published);}catch(...){return core::fail(core::ErrorCode::failed,"Web widget mount failed",std::current_exception());}}
core::Result<void> Host::unmount_widgets(){if(auto r=impl_->ready();!r)return r;if(impl_->mounting)return core::fail(core::ErrorCode::busy,"Widget factory active");if(impl_->widgets){impl_->widgets->close();impl_->widgets.reset();}return {};}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected){auto owner=expected.state_.lock();if(!owner)return core::fail(core::ErrorCode::closed,"Widget generation retired");if(owner!=impl_->widgets)return core::fail(core::ErrorCode::invalid_argument,"Widget generation belongs to another host");return unmount_widgets();}
core::Result<void> Host::clear_canvas(){if(auto r=impl_->ready();!r)return r;impl_->canvas.clear();return {};}
core::Result<GraphicsInfo> Host::graphics_info()const{if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->graphics_info():core::Result<GraphicsInfo>{impl_->canvas.info()};}
core::Result<PixelBuffer> Host::capture_canvas(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->widgets?impl_->widgets->capture():impl_->canvas.capture(impl_->surface.scale,impl_->surface.size.height);}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop(){if(auto r=impl_->ready();!r)return std::unexpected(r.error());return std::ref(impl_->dispatch->loop);}
core::Result<WindowSnapshot> Host::snapshot()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Snapshot requires host owner");if(impl_->phase==Impl::Phase::created)return core::fail(core::ErrorCode::closed,"Host not started");return impl_->state;}
core::Result<DisplayPixels> Host::display_pixels()const{if(auto ready=impl_->ready();!ready)return std::unexpected(ready.error());return core::fail(core::ErrorCode::unsupported,"The browser cannot reliably identify the physical display mode");}
core::Result<HostStats> Host::stats()const{if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Stats require host owner");auto result=impl_->statistics;result.loop=impl_->dispatch->loop.stats();result.host=impl_->dispatch->host_stats();return result;}
}
