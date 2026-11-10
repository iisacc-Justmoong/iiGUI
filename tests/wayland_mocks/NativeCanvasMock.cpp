#include <iiGUI/NativeCanvas.h>

// Headless graphics substitute for tests of the actual Wayland surface.
namespace ii::gui::detail {
struct NativeCanvas::Impl final {};
NativeCanvas::NativeCanvas(void*,void*,Acceleration,Renderer,std::shared_ptr<PresentationStream>){}
NativeCanvas::~NativeCanvas()=default;
void NativeCanvas::prepare(const CanvasFrame&){}
void NativeCanvas::draw(const CanvasFrame&,double,double){}
PixelBuffer NativeCanvas::capture(){return {};}
GraphicsInfo NativeCanvas::info()const{return {};}
bool NativeCanvas::native_event(const void*){return false;}
}
