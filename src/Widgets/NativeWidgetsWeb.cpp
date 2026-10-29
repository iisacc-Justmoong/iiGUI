#include <iiGUI/WebSurface.h>
#include <iiGUI/WidgetSurface.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/FontDatabase.h>
namespace ii::gui::detail {
struct WidgetSurface::Impl final {
    WebSurfaceParent& parent;WidgetSurfaceCallbacks callbacks;std::unique_ptr<NativeCanvas> canvas;bool dirty=true,lost=false,live=true;
    Impl(WebSurfaceParent& p,WidgetSurfaceCallbacks c):parent(p),callbacks(std::move(c)){reset();}
    void reset(){canvas.reset();canvas=std::make_unique<NativeCanvas>(&parent,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations);dirty=true;lost=false;}
    void flush(){if(!live||!dirty||lost||!parent.visible||!canvas)return;auto frame=callbacks.frame(parent.size);if(!live)return;canvas->draw(frame,parent.scale,parent.size.height);dirty=false;}
};
WidgetSurface::WidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<WebSurfaceParent*>(parent),std::move(callbacks))){}
WidgetSurface::~WidgetSurface(){impl_->live=false;}
void WidgetSurface::invalidate(){impl_->dirty=true;if(impl_->parent.wake)web_wake(*impl_->parent.wake);}
void WidgetSurface::accessibility_changed(){}
void WidgetSurface::accessibility_value_changed(std::string_view){}
void WidgetSurface::set_frame_paced(bool){}
void WidgetSurface::flush(){auto state=impl_;state->flush();}
Size WidgetSurface::size()const{return impl_->parent.size;}
double WidgetSurface::backing_scale()const{return impl_->parent.scale;}
std::string WidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas?impl_->canvas->info():GraphicsInfo{};}
PixelBuffer WidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live||!state->canvas||state->lost)throw std::runtime_error("WebGL2 context is lost or retired");return state->canvas->capture();}
bool WidgetSurface::native_event(const void* raw){auto state=impl_;try{const auto& event=*static_cast<const WebSurfaceEvent*>(raw);if(event.kind==WebSurfaceEventKind::lost)state->lost=true;else if(event.kind==WebSurfaceEventKind::restore)state->reset();else if(event.input)state->callbacks.input(*event.input);if(state->live){state->dirty=true;if(state->parent.wake)web_wake(*state->parent.wake);}return true;}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
