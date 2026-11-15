#include "DirectCompositionSurface.h"
#include "CompositionResize.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT value,const char* message){
    if(FAILED(value))throw std::runtime_error(std::string(message)+" (HRESULT "+std::to_string(value)+")");
}
}
struct DirectCompositionSurface::Impl final {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> chain;
    ComPtr<IDCompositionDevice> composition;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;
    CompositionResize size{1,1};
    ~Impl(){
        if(target && composition){(void)target->SetRoot(nullptr);(void)composition->Commit();}
        visual.Reset();target.Reset();composition.Reset();
        if(context){context->ClearState();context->Flush();}
        chain.Reset();
    }
    void initialize(HWND parent){
        if(!parent)throw std::invalid_argument("A borrowed parent window is required");
        const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL selected{};
        checked(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,1,D3D11_SDK_VERSION,&device,&selected,&context),"Cannot create Direct3D composition device");
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;
        checked(device.As(&dxgi),"Cannot access composition DXGI device");
        checked(dxgi->GetAdapter(&adapter),"Cannot access composition adapter");
        checked(adapter->GetParent(IID_PPV_ARGS(&factory)),"Cannot access composition DXGI factory");
        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width=description.Height=1;
        description.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count=1;
        description.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount=2;
        description.Scaling=DXGI_SCALING_STRETCH;
        description.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        description.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
        checked(factory->CreateSwapChainForComposition(device.Get(),&description,nullptr,&chain),"Cannot create transparent composition swapchain");
        checked(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&composition)),"Cannot create DirectComposition device");
        // Use the topmost visual tree so an embedded modal is not clipped by
        // native child surfaces owned by the application. Windows permits one
        // topmost target per HWND; an existing target is a reported failure.
        checked(composition->CreateTargetForHwnd(parent,TRUE,&target),"Cannot attach DirectComposition to external parent");
        checked(composition->CreateVisual(&visual),"Cannot create DirectComposition visual");
        checked(visual->SetContent(chain.Get()),"Cannot bind DirectComposition swapchain");
        checked(target->SetRoot(visual.Get()),"Cannot install DirectComposition visual");
        checked(composition->Commit(),"Cannot commit DirectComposition visual");
    }
};
DirectCompositionSurface::DirectCompositionSurface(void* parent):impl_(std::make_unique<Impl>()){
    impl_->initialize(static_cast<HWND>(parent));
}
DirectCompositionSurface::~DirectCompositionSurface()=default;
void DirectCompositionSurface::resize(std::uint32_t width,std::uint32_t height){
    (void)impl_->size.update(width,height,[&](auto w,auto h){
        impl_->context->ClearState();impl_->context->Flush();
        checked(impl_->chain->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0),"Cannot resize transparent composition swapchain");
    });
}
void* DirectCompositionSurface::swapchain()const noexcept{return impl_->chain.Get();}
} // namespace ii::gui::detail
