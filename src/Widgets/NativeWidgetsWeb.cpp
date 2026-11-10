#include <iiGUI/WebSurface.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/FontDatabase.h>
namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    WebSurfaceParent& parent;WidgetSurfaceCallbacks callbacks;std::unique_ptr<NativeCanvas> canvas;bool dirty=true,lost=false,live=true;
    Impl(WebSurfaceParent& p,WidgetSurfaceCallbacks c):parent(p),callbacks(std::move(c)){reset();}
    void reset(){canvas.reset();canvas=std::make_unique<NativeCanvas>(&parent,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations);dirty=true;lost=false;}
    void flush(){if(!live||!dirty||lost||!parent.visible||!canvas)return;auto frame=callbacks.frame(parent.size);if(!live)return;canvas->draw(frame,parent.scale,parent.size.height);dirty=false;}
};
NativeWidgetSurface::NativeWidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<WebSurfaceParent*>(parent),std::move(callbacks))){}
NativeWidgetSurface::~NativeWidgetSurface(){impl_->live=false;}
void NativeWidgetSurface::invalidate(){impl_->dirty=true;if(impl_->parent.wake)web_wake(*impl_->parent.wake);}
void NativeWidgetSurface::accessibility_changed(){}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){}
void NativeWidgetSurface::set_frame_paced(bool){}
void NativeWidgetSurface::flush(){auto state=impl_;state->flush();}
Size NativeWidgetSurface::size()const{return impl_->parent.size;}
double NativeWidgetSurface::backing_scale()const{return impl_->parent.scale;}
std::string NativeWidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo NativeWidgetSurface::info(){return impl_->canvas?impl_->canvas->info():GraphicsInfo{};}
PixelBuffer NativeWidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live||!state->canvas||state->lost)throw std::runtime_error("WebGL2 context is lost or retired");return state->canvas->capture();}
bool NativeWidgetSurface::native_event(const void* raw){auto state=impl_;try{const auto& event=*static_cast<const WebSurfaceEvent*>(raw);if(event.kind==WebSurfaceEventKind::lost)state->lost=true;else if(event.kind==WebSurfaceEventKind::restore)state->reset();else if(event.input)state->callbacks.input(*event.input);if(state->live){state->dirty=true;if(state->parent.wake)web_wake(*state->parent.wake);}return true;}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
