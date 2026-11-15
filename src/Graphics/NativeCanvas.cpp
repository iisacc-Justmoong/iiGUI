#include <iiGUI/NativeCanvas.h>
#if !defined(__EMSCRIPTEN__)
#include <iiGUI/VulkanCanvas.h>
#endif
#include <iiGUI/RendererChain.h>
#include <algorithm>
#include <iiGUI/SurfaceRegistry.h>
#include <iiGUI/PresentationBoundary.h>
#if defined(_WIN32)
#include "DirectCompositionSurface.h"
#endif

namespace ii::gui::detail {
struct NativeCanvas::Impl final {
#if defined(_WIN32)
    // The renderer releases its swapchain reference before the target detaches.
    std::unique_ptr<DirectCompositionSurface> composition;
#endif
#if defined(__EMSCRIPTEN__)
    PlatformCanvas renderer;
#elif defined(__APPLE__) || defined(_WIN32)
    RendererChain<PlatformCanvas,VulkanCanvas> renderer;
#else
    RendererChain<VulkanCanvas,VulkanCanvas,false> renderer;
#endif
    Impl(void* parent,void* display,Acceleration acceleration,Renderer selection)
#if defined(__EMSCRIPTEN__)
        : renderer(parent,display,acceleration) {if(selection==Renderer::vulkan)throw std::runtime_error("Browser host requires WebGL2; Vulkan is an OS host API");}
#else
        : renderer(parent,display,acceleration,selection) {}
#endif
#if defined(_WIN32)
    Impl(NativeCanvas::DirectCompositionParent parent,Acceleration acceleration)
        :composition(std::make_unique<DirectCompositionSurface>(parent.window)),
         renderer(std::make_unique<PlatformCanvas>(PlatformCanvas::ExternalDirect3DSwapChain{composition->swapchain(),true},acceleration),acceleration) {}
#endif
};
NativeCanvas::NativeCanvas(void* p,void* d,Acceleration a,Renderer r,std::shared_ptr<PresentationStream> presentations):presentations_(std::move(presentations)) {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || defined(IIGUI_WAYLAND)
#if defined(__EMSCRIPTEN__)
    if(r==Renderer::vulkan)throw std::runtime_error("Web host requires WebGL2");
#else
    if(r==Renderer::native)throw std::runtime_error("This OS host uses Vulkan");
#endif
    static thread_local SurfaceRegistry<Impl> surfaces;
    impl_=surfaces.acquire(p,d,[&]{return std::make_shared<Impl>(p,d,a,r);});
    if(a==Acceleration::require_hardware&&!impl_->renderer.info().hardware)
        throw std::runtime_error("The shared native surface cannot guarantee hardware acceleration");
#else
    impl_=std::make_shared<Impl>(p,d,a,r);
#endif
}
#if defined(_WIN32)
NativeCanvas::NativeCanvas(DirectCompositionParent parent,Acceleration policy,std::shared_ptr<PresentationStream> presentations)
    :impl_(std::make_shared<Impl>(parent,policy)),presentations_(std::move(presentations)) {}
#endif
NativeCanvas::~NativeCanvas()=default;
void NativeCanvas::prepare(const CanvasFrame& frame) {
#if defined(__EMSCRIPTEN__)
    const auto plan=plan_canvas_scenes(frame);
    if(!plan)throw std::invalid_argument("Invalid GPU source scene graph");
    if(plan->count&&!impl_->renderer.info().scene_sources_supported)throw std::runtime_error("GPU source scenes are not supported by this renderer");
#else
    impl_->renderer.prepare(frame);
#endif
}
void NativeCanvas::draw(const CanvasFrame& f,double s,double h) {
    prepare(f);
    if(std::ranges::any_of(f.overlays,[](const auto& l){return bool(l.backdrop);})&&!impl_->renderer.info().backdrop_supported)
        throw std::runtime_error("GPU backdrop materials require a supported accelerator");
    if(std::ranges::any_of(f.overlays,[](const auto& l){return bool(l.shadow);})&&!impl_->renderer.info().shadow_supported)
        throw std::runtime_error("GPU box shadows require a supported accelerator");
    if(std::ranges::any_of(f.overlays,[](const auto& l){return !l.content_transform.identity();})&&!impl_->renderer.info().content_transform_supported)
        throw std::runtime_error("GPU content transforms require a supported accelerator");
    auto state=impl_;auto source=presentations_;
    draw_with_presentation(source,[&]{
#if defined(_WIN32)
        if(state->composition){const auto [width,height]=canvas_pixels(f,s);state->composition->resize(width,height);}
#endif
        state->renderer.draw(f,s,h);
    },[&]{return state->renderer.info();},[]{return runtime::Clock::now();});
}
PixelBuffer NativeCanvas::capture() { return impl_->renderer.capture(); }
GraphicsInfo NativeCanvas::info() const { return impl_->renderer.info(); }
bool NativeCanvas::native_event(const void* e) { return impl_->renderer.native_event(e); }
} // namespace ii::gui::detail
