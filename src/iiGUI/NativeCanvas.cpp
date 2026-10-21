#include "NativeCanvas.h"
#include "VulkanCanvas.h"
#include "RendererChain.h"

namespace ii::gui::detail {
struct NativeCanvas::Impl final {
#if defined(__APPLE__) || defined(_WIN32)
    RendererChain<PlatformCanvas,VulkanCanvas> renderer;
#else
    RendererChain<VulkanCanvas,VulkanCanvas,false> renderer;
#endif
    Impl(void* parent,void* display,Acceleration acceleration,Renderer selection)
        : renderer(parent,display,acceleration,selection) {}
};
NativeCanvas::NativeCanvas(void* p,void* d,Acceleration a,Renderer r):impl_(std::make_unique<Impl>(p,d,a,r)) {}
NativeCanvas::~NativeCanvas()=default;
void NativeCanvas::draw(const CanvasFrame& f,double s,double h) { impl_->renderer.draw(f,s,h); }
PixelBuffer NativeCanvas::capture() { return impl_->renderer.capture(); }
GraphicsInfo NativeCanvas::info() const { return impl_->renderer.info(); }
bool NativeCanvas::native_event(const void* e) { return impl_->renderer.native_event(e); }
} // namespace ii::gui::detail
