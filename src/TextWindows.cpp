#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "NativeText.h"

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT value,const char* operation) {
    if (FAILED(value)) throw std::runtime_error(std::string(operation)+" (HRESULT "+std::to_string(value)+")");
}
struct Apartment final {
    HRESULT status=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Apartment() { if (status!=RPC_E_CHANGED_MODE) checked(status,"Cannot initialize text COM apartment"); }
    ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
};
std::wstring wide(std::string_view value) {
    if (value.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if (!size) throw std::runtime_error("Invalid UTF-8 native text");
    std::wstring result(std::size_t(size),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),size);
    return result;
}
ComPtr<IDWriteTextLayout> layout(IDWriteFactory* factory,const TextRun& run) {
    ComPtr<IDWriteTextFormat> format;
    checked(factory->CreateTextFormat(L"Segoe UI",nullptr,run.font_weight==700?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,run.font_size,L"",&format),"Cannot create native text format");
    checked(format->SetTextAlignment(run.alignment==TextAlignment::center?DWRITE_TEXT_ALIGNMENT_CENTER:
        run.alignment==TextAlignment::right?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING),"Cannot align native text");
    checked(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP),"Cannot wrap native text");
    auto text=wide(run.text); ComPtr<IDWriteTextLayout> result;
    checked(factory->CreateTextLayout(text.data(),UINT32(text.size()),format.Get(),run.bounds.width,8192,&result),"Cannot shape native text");
    return result;
}
ComPtr<IDWriteFactory> text_factory() {
    ComPtr<IDWriteFactory> result;
    checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(result.GetAddressOf())),"Cannot create native text factory");
    return result;
}
}
float measure_text(const TextRun& run) {
    auto factory=text_factory(); auto text=layout(factory.Get(),run); DWRITE_TEXT_METRICS metrics{};
    checked(text->GetMetrics(&metrics),"Cannot measure native text");
    return std::max(run.font_size,float(std::ceil(metrics.height)));
}
PixelBuffer rasterize_text(const CanvasFrame& frame,double scale) {
    Apartment apartment;
    const auto [width,height]=canvas_pixels(frame,scale);
    ComPtr<IWICImagingFactory> imaging;
    checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging)),"Cannot create native text bitmap factory");
    ComPtr<IWICBitmap> bitmap;
    checked(imaging->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap),"Cannot allocate native text bitmap");
    ComPtr<ID2D1Factory> drawing_factory;
    checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawing_factory.GetAddressOf()),"Cannot create native text drawing factory");
    ComPtr<ID2D1RenderTarget> drawing;
    auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),float(96*scale),float(96*scale));
    checked(drawing_factory->CreateWicBitmapRenderTarget(bitmap.Get(),properties,&drawing),"Cannot draw to native text bitmap");
    auto factory=text_factory(); ComPtr<ID2D1SolidColorBrush> brush;
    checked(drawing->CreateSolidColorBrush(D2D1::ColorF(0,0,0,1),&brush),"Cannot create native text brush");
    drawing->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    drawing->BeginDraw(); drawing->Clear(D2D1::ColorF(0,0,0,0));
    for (const auto& run:frame.texts) {
        auto c=run.color; brush->SetColor(D2D1::ColorF(encoded_srgb(c.red),encoded_srgb(c.green),encoded_srgb(c.blue),c.alpha));
        auto r=run.bounds; auto text=layout(factory.Get(),run);
        drawing->PushAxisAlignedClip(D2D1::RectF(r.x,r.y,r.x+r.width,r.y+r.height),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        drawing->DrawTextLayout(D2D1::Point2F(r.x,r.y),text.Get(),brush.Get()); drawing->PopAxisAlignedClip();
    }
    checked(drawing->EndDraw(),"Native text drawing failed");
    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    checked(bitmap->CopyPixels(nullptr,width*4,UINT(result.rgba.size()),result.rgba.data()),"Cannot access native text pixels");
    straight_rgba(result,true); return result;
}
} // namespace ii::gui::detail
