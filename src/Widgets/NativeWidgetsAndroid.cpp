#include <iiGUI/FontDatabase.h>
#include <iiGUI/AndroidSurface.h>
#include <iiGUI/WidgetSurface.h>
#include <iiGUI/NativeCanvas.h>
#include <android/looper.h>

namespace ii::gui::detail {
struct WidgetSurface::Impl final {
    AndroidSurfaceParent& parent;
    WidgetSurfaceCallbacks callbacks;
    std::unique_ptr<NativeCanvas> canvas;
    bool dirty=true,live=true;
    Impl(AndroidSurfaceParent& p,WidgetSurfaceCallbacks c):parent(p),callbacks(std::move(c)){reset();}
    void reset(){canvas.reset();if(parent.window)canvas=std::make_unique<NativeCanvas>(parent.window,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations);dirty=true;}
    Size size()const{return parent.window?Size{ANativeWindow_getWidth(parent.window)/parent.scale,ANativeWindow_getHeight(parent.window)/parent.scale}:Size{};}
    void invalidate(){if(!live)return;dirty=true;ALooper_wake(parent.app->looper);}
    void flush(){if(!live||!dirty||!canvas||!parent.active)return;const auto extent=size();if(extent.width<=0||extent.height<=0)return;auto frame=callbacks.frame(extent);if(!live)return;canvas->draw(frame,parent.scale,extent.height);dirty=false;}
    bool event(const AndroidSurfaceEvent& e){
        if(e.kind==AndroidSurfaceEventKind::window){reset();return true;}
        if(e.kind==AndroidSurfaceEventKind::resize||e.kind==AndroidSurfaceEventKind::activation){invalidate();return true;}
        if(!e.input||AInputEvent_getType(e.input)!=AINPUT_EVENT_TYPE_MOTION)return false;
        const auto action=AMotionEvent_getAction(e.input)&AMOTION_EVENT_ACTION_MASK;
        if(action==AMOTION_EVENT_ACTION_CANCEL){callbacks.input({.kind=WidgetInputKind::focus_lost});return true;}
        if(action!=AMOTION_EVENT_ACTION_DOWN&&action!=AMOTION_EVENT_ACTION_MOVE&&action!=AMOTION_EVENT_ACTION_UP)return false;
        WidgetInput input{.kind=action==AMOTION_EVENT_ACTION_DOWN?WidgetInputKind::pointer_down:action==AMOTION_EVENT_ACTION_UP?WidgetInputKind::pointer_up:WidgetInputKind::pointer_move};
        input.x=AMotionEvent_getX(e.input,0)/parent.scale;input.y=AMotionEvent_getY(e.input,0)/parent.scale;
        input.device=PointerDevice::touch;input.device_id=std::uint64_t(AMotionEvent_getPointerId(e.input,0));
        input.pressure=std::clamp(double(AMotionEvent_getPressure(e.input,0)),0.,1.);input.buttons=action==AMOTION_EVENT_ACTION_UP?0:1;
        input.timestamp=AMotionEvent_getEventTime(e.input)/1e9;callbacks.input(std::move(input));invalidate();return true;
    }
};
WidgetSurface::WidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<AndroidSurfaceParent*>(parent),std::move(callbacks))){}
WidgetSurface::~WidgetSurface(){impl_->live=false;}
void WidgetSurface::invalidate(){impl_->invalidate();}
void WidgetSurface::accessibility_changed(){}
void WidgetSurface::accessibility_value_changed(std::string_view){}
void WidgetSurface::set_frame_paced(bool){}
void WidgetSurface::flush(){auto state=impl_;state->flush();}
Size WidgetSurface::size()const{return impl_->size();}
double WidgetSurface::backing_scale()const{return impl_->parent.scale;}
std::string WidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas?impl_->canvas->info():GraphicsInfo{};}
PixelBuffer WidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live||!state->canvas)throw std::runtime_error("Android surface is suspended or retired");return state->canvas->capture();}
bool WidgetSurface::native_event(const void* event){auto state=impl_;try{return state->event(*static_cast<const AndroidSurfaceEvent*>(event));}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
