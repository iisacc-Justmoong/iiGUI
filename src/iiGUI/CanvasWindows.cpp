#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "NativeCanvas.h"
#include "NativeText.h"
#include "GpuEffects.h"
#include <optional>
#include <cstring>

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT value, const char* operation) {
    if (FAILED(value)) throw std::runtime_error(std::string(operation)+" (HRESULT "+std::to_string(value)+")");
}
const char shader[] = R"hlsl(
struct Panel { float4 bounds, top, bottom; float radius,stroke; float2 padding; };
StructuredBuffer<Panel> panels : register(t0);
cbuffer View : register(b0) { float2 size; float2 padding; float4 effect_bounds, effect_tint, effect_border, effect_style; };
struct Varying { float4 position : SV_POSITION; float2 local : TEXCOORD0;
    float2 size : TEXCOORD1; float4 top : COLOR0; float4 bottom : COLOR1; float radius : TEXCOORD2; float stroke : TEXCOORD3; };
Varying vertex_main(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
    const float2 corners[] = {float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    Panel p=panels[instance]; float2 uv=corners[vertex]; float2 xy=p.bounds.xy+uv*p.bounds.zw;
    Varying v; v.position=float4(xy.x/size.x*2-1,1-xy.y/size.y*2,0,1);
    v.local=uv*p.bounds.zw; v.size=p.bounds.zw; v.top=p.top; v.bottom=p.bottom; v.radius=p.radius;v.stroke=p.stroke; return v;
}
float4 fragment_main(Varying v) : SV_TARGET {
    float2 q=abs(v.local-v.size*0.5)-(v.size*0.5-v.radius);
    float distance=length(max(q,0))+min(max(q.x,q.y),0)-v.radius;
    float coverage=saturate(0.5-distance/max(abs(ddx(distance))+abs(ddy(distance)),0.0001));
    if(v.stroke>0){float radius=max(0,v.radius-v.stroke);float2 q=abs(v.local-v.size*.5)-(v.size*.5-v.stroke-radius);float d=length(max(q,0))+min(max(q.x,q.y),0)-radius;coverage=max(0,coverage-saturate(.5-d/max(abs(ddx(d))+abs(ddy(d)),.0001)));}
    float4 color=lerp(v.top,v.bottom,saturate(v.local.y/v.size.y));
    color.a*=coverage; return float4(color.rgb*color.a,color.a);
}
Texture2D<float4> glyphs : register(t1);
float4 text_vertex(uint vertex : SV_VertexID) : SV_POSITION {
    const float2 corners[]={float2(-1,1),float2(1,1),float2(-1,-1),float2(-1,-1),float2(1,1),float2(1,-1)};
    return float4(corners[vertex],0,1);
}
float4 text_fragment(float4 position : SV_POSITION) : SV_TARGET {
    uint width,height;glyphs.GetDimensions(width,height);int2 pixel=int2(position.xy-effect_bounds.xy);if(any(pixel<0)||any(pixel>=int2(width,height)))return 0;
    float4 color=glyphs.Load(int3(pixel,0));
    return float4(color.rgb*color.a,color.a);
}

SamplerState linear_clamp : register(s0);
float shape_distance(float2 point,float4 bounds,float radius) {
    float2 q=abs(point-bounds.xy-bounds.zw*.5)-(bounds.zw*.5-radius);
    return length(max(q,0))+min(max(q.x,q.y),0)-radius;
}
float shape_mask(float2 point) { return saturate(.5-shape_distance(point,effect_bounds,effect_style.x)*padding.x); }
float4 effect_fragment(float4 position : SV_POSITION) : SV_TARGET {
    float scale=padding.x,opacity=padding.y,blur=effect_style.y;
    float2 point=position.xy/scale;
    bool shadow=effect_style.w>.5;
    if(opacity<=0||shape_distance(point,effect_bounds,effect_style.x)>(shadow?blur+1:1))return 0;
    float coverage=shape_mask(point);float3 paint;
    if(shadow){
        if(blur>.01){coverage=0;float total=0,sigma=blur/3,step_size=blur/6;
            [loop] for(int y=-6;y<=6;++y)[loop] for(int x=-6;x<=6;++x){float2 offset=float2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));coverage+=shape_mask(point+offset)*weight;total+=weight;}coverage/=total;}
        float alpha=coverage*effect_tint.a*opacity;return float4(effect_tint.rgb*alpha,alpha);
    }
    float2 extent=size*scale;
    if(blur>.01){paint=0;float total=0,sigma=blur/3,step_size=blur/6;
        [loop] for(int y=-6;y<=6;++y)[loop] for(int x=-6;x<=6;++x){float2 offset=float2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));paint+=glyphs.SampleLevel(linear_clamp,(position.xy+offset*scale)/extent,0).rgb*weight;total+=weight;}paint/=total;
    }else paint=glyphs.SampleLevel(linear_clamp,position.xy/extent,0).rgb;
    paint=lerp(paint,effect_tint.rgb,effect_tint.a);
    float border_width=effect_style.z;
    if(border_width>0&&effect_border.a>0){float4 inner=effect_bounds+float4(border_width,border_width,-2*border_width,-2*border_width);float inside=saturate(.5-shape_distance(point,inner,max(0,effect_style.x-border_width))*scale);paint=lerp(paint,effect_border.rgb,saturate((coverage-inside)/max(coverage,.0001))*effect_border.a);}
    float alpha=coverage*opacity;return float4(paint*alpha,alpha);
}
)hlsl";
ComPtr<ID3DBlob> compile(const char* entry, const char* profile) {
    ComPtr<ID3DBlob> code, errors;
    auto result=D3DCompile(shader,sizeof(shader)-1,"iiGUI panels",nullptr,nullptr,entry,profile,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if (FAILED(result) && errors) throw std::runtime_error(std::string("D3D11 shader: ")+
        std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()));
    checked(result,"Cannot compile native D3D11 shader"); return code;
}
std::string utf8(const wchar_t* text) {
    const int size=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    if (size<=1) return "Direct3D 11 adapter";
    std::string value(static_cast<std::size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text,-1,value.data(),size,nullptr,nullptr);
    value.pop_back(); return value;
}
}
struct PlatformCanvas::Impl final {
    HWND window=nullptr;
    HINSTANCE instance=GetModuleHandleW(nullptr);
    std::wstring class_name;
    bool registered=false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapchain;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> fragment;
    ComPtr<ID3D11VertexShader> text_vertex;
    ComPtr<ID3D11PixelShader> text_fragment, effect_fragment;
    ComPtr<ID3D11SamplerState> effect_sampler;
    ComPtr<ID3D11Texture2D> content_snapshot, current_snapshot;
    ComPtr<ID3D11ShaderResourceView> content_view, current_view;
    ComPtr<ID3D11Buffer> view;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    struct Layer final { ComPtr<ID3D11ShaderResourceView> foreground; ForegroundKey key;PixelClip region{}; };
    std::vector<Layer> layers;
    std::optional<CanvasFrame> previous;
    double previous_scale=0;
    GraphicsInfo statistics;

    ~Impl() {
        if (context) { context->ClearState(); context->Flush(); }
        swapchain.Reset();
        if (window) DestroyWindow(window);
        if (registered) UnregisterClassW(class_name.c_str(),instance);
    }
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) noexcept {
        if(message==WM_NCHITTEST)return HTTRANSPARENT;
        if (message==WM_ERASEBKGND) return 1;
        if (message==WM_PAINT) { PAINTSTRUCT p{}; BeginPaint(window,&p); EndPaint(window,&p); return 0; }
        return DefWindowProcW(window,message,w,l);
    }
    void initialize(HWND parent,Acceleration) {
        const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL level{};
        auto result=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,1,D3D11_SDK_VERSION,&device,&level,&context);
        statistics.hardware=SUCCEEDED(result);
        checked(result,"No suitable Direct3D 11 hardware device");
        ComPtr<IDXGIDevice> dxgi;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory> factory;
        checked(device.As(&dxgi),"Cannot access DXGI device");
        checked(dxgi->GetAdapter(&adapter),"Cannot access DXGI adapter");
        DXGI_ADAPTER_DESC description{};
        checked(adapter->GetDesc(&description),"Cannot identify DXGI adapter");
        ComPtr<IDXGIAdapter1> adapter1;
        if (SUCCEEDED(adapter.As(&adapter1))) {
            DXGI_ADAPTER_DESC1 details{};
            checked(adapter1->GetDesc1(&details),"Cannot query DXGI adapter flags");
            if (details.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) statistics.hardware=false;
        }
        if (!statistics.hardware)
            throw std::runtime_error("DXGI adapter reports software rendering; native hardware acceleration is unavailable");
        checked(adapter->GetParent(IID_PPV_ARGS(&factory)),"Cannot access DXGI factory");
        statistics.api=GraphicsApi::direct3d11;
        statistics.adapter=utf8(description.Description);
        statistics.srgb_attachment=true;
        statistics.backdrop_supported=statistics.shadow_supported=statistics.content_transform_supported=true;
        class_name=L"iiGUI.Canvas."+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
        WNDCLASSEXW type{}; type.cbSize=sizeof(type); type.lpfnWndProc=procedure;
        type.hInstance=instance; type.lpszClassName=class_name.c_str();
        if (!RegisterClassExW(&type)) throw std::runtime_error("Cannot register D3D11 child window");
        registered=true;
        window=CreateWindowExW(0,class_name.c_str(),L"",WS_CHILD|WS_CLIPSIBLINGS,0,0,1,1,parent,nullptr,instance,nullptr);
        if (!window) throw std::runtime_error("Cannot create D3D11 child surface");
        DXGI_SWAP_CHAIN_DESC swap{};
        swap.BufferDesc.Width=swap.BufferDesc.Height=1;
        swap.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        swap.SampleDesc.Count=1; swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap.BufferCount=2; swap.OutputWindow=window; swap.Windowed=TRUE;
        swap.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        checked(factory->CreateSwapChain(device.Get(),&swap,&swapchain),"Cannot create D3D11 flip swapchain");
        checked(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"Cannot set DXGI window association");
        auto vs=compile("vertex_main","vs_5_0"), ps=compile("fragment_main","ps_5_0");
        checked(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex),"Cannot create D3D11 vertex pipeline");
        checked(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&fragment),"Cannot create D3D11 pixel pipeline");
        auto tvs=compile("text_vertex","vs_5_0"), tps=compile("text_fragment","ps_5_0");
        checked(device->CreateVertexShader(tvs->GetBufferPointer(),tvs->GetBufferSize(),nullptr,&text_vertex),"Cannot create D3D11 text vertex pipeline");
        checked(device->CreatePixelShader(tps->GetBufferPointer(),tps->GetBufferSize(),nullptr,&text_fragment),"Cannot create D3D11 text pixel pipeline");
        auto effect=compile("effect_fragment","ps_5_0");
        checked(device->CreatePixelShader(effect->GetBufferPointer(),effect->GetBufferSize(),nullptr,&effect_fragment),"Cannot create D3D11 material pipeline");
        D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
        checked(device->CreateSamplerState(&sampler,&effect_sampler),"Cannot create D3D11 material sampler");
        D3D11_BUFFER_DESC uniform{}; uniform.ByteWidth=sizeof(GpuEffect); uniform.Usage=D3D11_USAGE_DEFAULT;
        uniform.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        checked(device->CreateBuffer(&uniform,nullptr,&view),"Cannot create D3D11 viewport constant buffer");
        D3D11_BLEND_DESC blending{};
        auto& b=blending.RenderTarget[0]; b.BlendEnable=TRUE;
        b.SrcBlend=b.SrcBlendAlpha=D3D11_BLEND_ONE;
        b.DestBlend=b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
        b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD; b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        checked(device->CreateBlendState(&blending,&blend),"Cannot create D3D11 linear blend state");
        D3D11_RASTERIZER_DESC raster{}; raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE;
        raster.DepthClipEnable=TRUE;raster.ScissorEnable=TRUE;
        checked(device->CreateRasterizerState(&raster,&rasterizer),"Cannot create D3D11 rasterizer state");
    }
    void draw(const CanvasFrame& frame,double scale) {
        const auto [width,height]=canvas_pixels(frame,scale);
        if (!SetWindowPos(window,nullptr,static_cast<int>(std::lround(frame.viewport.x*scale)),
            static_cast<int>(std::lround(frame.viewport.y*scale)),width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW))
            throw std::runtime_error("Cannot position D3D11 canvas child surface");
        bool changed=!previous || *previous!=frame || previous_scale!=scale;
        if (!texture || statistics.pixel_width!=width || statistics.pixel_height!=height) {
            context->OMSetRenderTargets(0,nullptr,nullptr);
            target.Reset(); texture.Reset();content_snapshot.Reset();current_snapshot.Reset();content_view.Reset();current_view.Reset();
            checked(swapchain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0),"Cannot resize D3D11 swapchain");
            D3D11_TEXTURE2D_DESC desc{}; desc.Width=width; desc.Height=height;
            desc.MipLevels=desc.ArraySize=1; desc.SampleDesc.Count=1;
            desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS; desc.Usage=D3D11_USAGE_DEFAULT;
            desc.BindFlags=D3D11_BIND_RENDER_TARGET;
            checked(device->CreateTexture2D(&desc,nullptr,&texture),"Cannot allocate D3D11 canvas attachment");
            D3D11_RENDER_TARGET_VIEW_DESC rtv{}; rtv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rtv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
            checked(device->CreateRenderTargetView(texture.Get(),&rtv,&target),"Cannot create D3D11 sRGB view");
            D3D11_TEXTURE2D_DESC snapshot=desc;snapshot.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            checked(device->CreateTexture2D(&snapshot,nullptr,&content_snapshot),"Cannot allocate D3D11 content snapshot");
            checked(device->CreateTexture2D(&snapshot,nullptr,&current_snapshot),"Cannot allocate D3D11 layer snapshot");
            D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
            checked(device->CreateShaderResourceView(content_snapshot.Get(),&srv,&content_view),"Cannot create D3D11 content sampler");
            checked(device->CreateShaderResourceView(current_snapshot.Get(),&srv,&current_view),"Cannot create D3D11 layer sampler");
            statistics.pixel_width=width; statistics.pixel_height=height;
            statistics.cached_texture_bytes=std::uint64_t(width)*height*12; changed=true;
        }
        if (changed) {
            auto* render=target.Get(); context->OMSetRenderTargets(1,&render,nullptr);
            const float clear[]{frame.background.red,frame.background.green,frame.background.blue,1};
            context->ClearRenderTargetView(render,clear);
            D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};
            context->RSSetViewports(1,&viewport); context->RSSetState(rasterizer.Get());
            context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            const GpuEffect size{{frame.viewport.width,frame.viewport.height,float(scale),1},{},{},{},{}};
            context->UpdateSubresource(view.Get(),0,nullptr,&size,0,0);
            auto* uniform=view.Get(); context->VSSetConstantBuffers(0,1,&uniform);context->PSSetConstantBuffers(0,1,&uniform);
            const auto views=canvas_layers(frame);layers.resize(views.size());statistics.foreground_texture_bytes=0;
            for(std::size_t i=0;i<views.size();++i) {
            const auto layer=views[i];auto& cached=layers[i];
            const auto snapshot=[&](ID3D11Texture2D* destination){context->OMSetRenderTargets(0,nullptr,nullptr);context->CopyResource(destination,texture.Get());context->OMSetRenderTargets(1,&render,nullptr);};
            if(i==1)snapshot(content_snapshot.Get());
            if(layer.backdrop&&layer.backdrop->source==BackdropSource::previous_layers)snapshot(current_snapshot.Get());
            const auto clip=clip_pixels(layer,scale,width,height);
            if(!clip.width || !clip.height) { cached.foreground.Reset();cached.key={};continue; }
            const D3D11_RECT scissor{LONG(clip.x),LONG(clip.y),LONG(clip.x+clip.width),LONG(clip.y+clip.height)};
            context->RSSetScissorRects(1,&scissor);
            const auto effect_draw=[&](bool shadow){const auto params=effect_parameters(layer,frame.viewport,scale,shadow);context->UpdateSubresource(view.Get(),0,nullptr,&params,0,0);
                auto* source=(!shadow&&layer.backdrop->source==BackdropSource::previous_layers)?current_view.Get():content_view.Get();
                context->PSSetShaderResources(1,1,&source);auto* sampler=effect_sampler.Get();context->PSSetSamplers(0,1,&sampler);
                context->VSSetShader(text_vertex.Get(),nullptr,0);context->PSSetShader(effect_fragment.Get(),nullptr,0);context->Draw(6,0);
                ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(1,1,&empty);};
            if(layer.shadow)effect_draw(true);if(layer.backdrop)effect_draw(false);
            context->UpdateSubresource(view.Get(),0,nullptr,&size,0,0);
            context->VSSetShader(vertex.Get(),nullptr,0); context->PSSetShader(fragment.Get(),nullptr,0);
            const auto panels=pack_panels(layer.panels,layer.content_transform);
            if (!panels.empty()) {
                D3D11_BUFFER_DESC desc{}; desc.ByteWidth=static_cast<UINT>(panels.size()*sizeof(GpuPanel));
                desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; desc.StructureByteStride=sizeof(GpuPanel);
                D3D11_SUBRESOURCE_DATA data{panels.data(),0,0};
                ComPtr<ID3D11Buffer> instances;
                checked(device->CreateBuffer(&desc,&data,&instances),"Cannot allocate D3D11 panel instances");
                D3D11_SHADER_RESOURCE_VIEW_DESC srv{}; srv.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
                srv.Buffer.NumElements=static_cast<UINT>(panels.size());
                ComPtr<ID3D11ShaderResourceView> resource;
                checked(device->CreateShaderResourceView(instances.Get(),&srv,&resource),"Cannot create D3D11 instance view");
                auto* input=resource.Get(); context->VSSetShaderResources(0,1,&input);
                context->DrawInstanced(6,static_cast<UINT>(panels.size()),0,0);
                ID3D11ShaderResourceView* empty=nullptr; context->VSSetShaderResources(0,1,&empty);
            }
            if (layer.foreground()) {
                if(!cached.foreground || !cached.key.matches(frame.viewport,scale,layer,true)) {
                auto raster=rasterize_foreground_region(frame.viewport,layer,scale,true);const auto& bitmap=raster.pixels;cached.region=raster.region;
                D3D11_TEXTURE2D_DESC desc{}; desc.Width=bitmap.width; desc.Height=bitmap.height;
                desc.MipLevels=desc.ArraySize=1; desc.SampleDesc.Count=1;
                desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; desc.Usage=D3D11_USAGE_IMMUTABLE;
                desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data{bitmap.rgba.data(),bitmap.width*4,0};
                ComPtr<ID3D11Texture2D> glyphs;
                checked(device->CreateTexture2D(&desc,&data,&glyphs),"Cannot allocate D3D11 text texture");
                ComPtr<ID3D11ShaderResourceView> resource;
                checked(device->CreateShaderResourceView(glyphs.Get(),nullptr,&resource),"Cannot create D3D11 foreground view");
                cached.foreground=std::move(resource);cached.key.assign(frame.viewport,scale,layer,true);
                ++statistics.foreground_rasterizations;
                }
                statistics.foreground_texture_bytes+=std::uint64_t(cached.region.width)*cached.region.height*4;
                auto foreground=size;foreground.bounds[0]=float(cached.region.x);foreground.bounds[1]=float(cached.region.y);context->UpdateSubresource(view.Get(),0,nullptr,&foreground,0,0);
                auto* input=cached.foreground.Get(); context->PSSetShaderResources(1,1,&input);
                context->VSSetShader(text_vertex.Get(),nullptr,0); context->PSSetShader(text_fragment.Get(),nullptr,0);
                context->Draw(6,0);
                ID3D11ShaderResourceView* empty=nullptr; context->PSSetShaderResources(1,1,&empty);
            } else { cached.foreground.Reset();cached.key={}; }
            }
            context->OMSetRenderTargets(0,nullptr,nullptr);
            ++statistics.scene_renders;
        }
        ComPtr<ID3D11Texture2D> back;
        checked(swapchain->GetBuffer(0,IID_PPV_ARGS(&back)),"Cannot access D3D11 presentation buffer");
        context->CopyResource(back.Get(),texture.Get());
        checked(swapchain->Present(1,0),"Cannot present D3D11 canvas (device may have been removed)");
        ++statistics.presentations; previous=frame;previous_scale=scale;
    }
    PixelBuffer capture() {
        if (!texture) throw std::runtime_error("D3D11 canvas has not rendered");
        D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.Usage=D3D11_USAGE_STAGING;
        desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        checked(device->CreateTexture2D(&desc,nullptr,&staging),"Cannot allocate D3D11 capture storage");
        context->CopyResource(staging.Get(),texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        checked(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Cannot read D3D11 canvas");
        PixelBuffer result{desc.Width,desc.Height,std::vector<std::uint8_t>(std::size_t(desc.Width)*desc.Height*4)};
        for (std::size_t y=0;y<desc.Height;++y) std::memcpy(result.rgba.data()+y*desc.Width*4,
            static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4);
        context->Unmap(staging.Get(),0); return result;
    }
};
PlatformCanvas::PlatformCanvas(void* parent,void*,Acceleration policy) : impl_(std::make_unique<Impl>()) {
    impl_->initialize(static_cast<HWND>(parent),policy);
}
PlatformCanvas::~PlatformCanvas()=default;
void PlatformCanvas::draw(const CanvasFrame& f,double s,double) { impl_->draw(f,s); }
PixelBuffer PlatformCanvas::capture() { return impl_->capture(); }
GraphicsInfo PlatformCanvas::info() const { return impl_->statistics; }
bool PlatformCanvas::native_event(const void*) { return false; }
} // namespace ii::gui::detail
