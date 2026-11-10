#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <iiGUI/TextLineNative.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/FontNative.h>

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("DirectWrite text line operation failed: " + std::to_string(hr)); }
std::vector<DWRITE_HIT_TEST_METRICS> ranges(IDWriteTextLayout* layout,std::size_t first,std::size_t last) {
    if(first==last)return {};
    UINT32 count=0;auto result=layout->HitTestTextRange(UINT32(first),UINT32(last-first),0,0,nullptr,0,&count);
    if(result!=E_NOT_SUFFICIENT_BUFFER)checked(result);
    std::vector<DWRITE_HIT_TEST_METRICS> hits(count);
    if(count)checked(layout->HitTestTextRange(UINT32(first),UINT32(last-first),0,0,hits.data(),count,&count));
    hits.resize(count);return hits;
}
TextBitmap raster(IDWriteTextLayout* line,double x,double top,double width,double height,Color color,double scale) {
    struct Apartment final { HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); Apartment() { if (hr != RPC_E_CHANGED_MODE) checked(hr); } ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); } } apartment;
    const auto w=UINT(std::ceil(width*scale)),h=UINT(std::ceil(height*scale));
    ComPtr<IWICImagingFactory> imaging;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging)));
    ComPtr<IWICBitmap> bitmap;checked(imaging->CreateBitmap(w,h,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap));
    ComPtr<ID2D1Factory> factory;checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf()));
    ComPtr<ID2D1RenderTarget> drawing;
    const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),float(96*scale),float(96*scale));
    checked(factory->CreateWicBitmapRenderTarget(bitmap.Get(),properties,&drawing));
    ComPtr<ID2D1SolidColorBrush> brush;checked(drawing->CreateSolidColorBrush(D2D1::ColorF(encoded_srgb(color.red),encoded_srgb(color.green),encoded_srgb(color.blue),color.alpha),&brush));
    drawing->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);drawing->BeginDraw();drawing->Clear(D2D1::ColorF(0,0,0,0));
    drawing->DrawTextLayout(D2D1::Point2F(float(-x),float(-top)),line,brush.Get(),D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);checked(drawing->EndDraw());
    PixelBuffer pixels{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
    checked(bitmap->CopyPixels(nullptr,w*4,UINT(pixels.rgba.size()),pixels.rgba.data()));straight_rgba(pixels,true);
    return {w,h,std::move(pixels.rgba)};
}
struct WrappedParagraph final {ComPtr<IDWriteTextLayout> layout;};
class WrappedLine final:public NativeTextLine {
    std::shared_ptr<WrappedParagraph> paragraph_;
    std::size_t first_,last_;
    double top_;
    TextLineMetrics metrics_;
public:
    WrappedLine(std::shared_ptr<WrappedParagraph> paragraph,std::size_t first,std::size_t last,double top,const DWRITE_LINE_METRICS& line)
        :paragraph_(std::move(paragraph)),first_(first),last_(last),top_(top) {
        double left=0,right=0;bool initial=true;
        for(const auto& hit:ranges(paragraph_->layout.Get(),first,last)){
            if(initial){left=hit.left;right=hit.left+hit.width;initial=false;}
            else{left=std::min(left,double(hit.left));right=std::max(right,double(hit.left+hit.width));}
        }
        metrics_={initial?0:right-left,std::max(1.,double(line.height)),line.baseline};
    }
    TextLineMetrics metrics()const override{return metrics_;}
    double caret(std::size_t position)const override {
        auto absolute=first_+position;const bool trailing=absolute==last_&&last_>first_;
        if(trailing)--absolute;
        FLOAT x=0,y=0;DWRITE_HIT_TEST_METRICS hit{};
        checked(paragraph_->layout->HitTestTextPosition(UINT32(absolute),trailing,&x,&y,&hit));return x;
    }
    std::size_t hit(double x)const override {
        BOOL trailing=FALSE,inside=FALSE;DWRITE_HIT_TEST_METRICS hit{};
        checked(paragraph_->layout->HitTestPoint(float(std::clamp(x,-1e9,1e9)),float(top_+metrics_.height/2),&trailing,&inside,&hit));
        const auto position=std::size_t(hit.textPosition)+(trailing?hit.length:0);
        return std::clamp(position,first_,last_)-first_;
    }
    std::vector<TextLineSpan> selection(std::size_t first,std::size_t last)const override {
        std::vector<TextLineSpan> result;
        for(const auto& hit:ranges(paragraph_->layout.Get(),first_+first,first_+last))result.push_back({hit.left,hit.width});
        return result;
    }
    TextBitmap render(double x,double width,Color color,double scale)const override {
        return raster(paragraph_->layout.Get(),x,top_,width,metrics_.height,color,scale);
    }
};
class Line final: public NativeTextLine {
    ComPtr<IDWriteTextLayout> line_;
    ComPtr<IDWriteFactory> factory_;
    ComPtr<IDWriteTextFormat> format_;
    std::wstring text_;
    TextLineMetrics metrics_;
public:
    Line(TextIndex display, const TextLineStyle& style) {
        const auto utf16 = [](std::string_view text) {
            if (text.empty()) return std::wstring{};
            const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
            if (!count) throw std::runtime_error("Invalid native text line encoding");
            std::wstring result(std::size_t(count), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), count); return result;
        };
        checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory_.GetAddressOf())));
        const auto family = style.font_family == "Monospace" || style.font_family == "monospace" ? L"Consolas" : utf16(style.font_family.empty()?FontDatabase::application()->resolve_family(""):style.font_family);
        auto collection=native_font_collection();
        checked(factory_->CreateTextFormat(family.c_str(), collection.Get(), DWRITE_FONT_WEIGHT(style.font_weight), DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, float(style.font_size), L"", &format_));
        checked(format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)); checked(format_->SetIncrementalTabStop(float(style.tab_stop_distance)));
        text_ = utf16(display.text());
        checked(factory_->CreateTextLayout(text_.data(), UINT32(text_.size()), format_.Get(), 1e8f, 8192, &line_));
        DWRITE_TEXT_METRICS measured{}; checked(line_->GetMetrics(&measured));
        DWRITE_LINE_METRICS lm{}; UINT32 count = 0; checked(line_->GetLineMetrics(&lm, 1, &count));
        metrics_ = {measured.widthIncludingTrailingWhitespace, style.line_height > 0 ? style.line_height : std::max(1., double(lm.height)), 0};
        metrics_.baseline = (metrics_.height - lm.height) / 2 + lm.baseline;
        checked(line_->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, float(metrics_.height), float(metrics_.baseline)));
    }
    TextLineMetrics metrics() const override { return metrics_; }
    double caret(std::size_t position) const override {
        FLOAT x = 0, y = 0; DWRITE_HIT_TEST_METRICS hit{};
        checked(line_->HitTestTextPosition(UINT32(position), FALSE, &x, &y, &hit)); return x;
    }
    std::size_t hit(double x) const override {
        BOOL trailing = FALSE, inside = FALSE; DWRITE_HIT_TEST_METRICS hit{};
        checked(line_->HitTestPoint(float(std::clamp(x, -1e9, 1e9)), float(metrics_.height / 2), &trailing, &inside, &hit));
        return std::size_t(hit.textPosition) + (trailing ? hit.length : 0);
    }
    std::vector<TextLineSpan> selection(std::size_t first, std::size_t last) const override {
        const auto hits=ranges(line_.Get(),first,last);
        std::vector<TextLineSpan> spans;spans.reserve(hits.size());
        for(const auto& h:hits)spans.push_back({h.left,h.width});return spans;
    }
    TextBitmap render(double x, double width, Color color, double scale) const override {
        return raster(line_.Get(),x,0,width,metrics_.height,color,scale);
    }
    std::vector<NativeLineSlice> wrap(double width,std::size_t maximum_lines)const override {
        auto paragraph=std::make_shared<WrappedParagraph>();
        checked(factory_->CreateTextLayout(text_.data(),UINT32(text_.size()),format_.Get(),float(width),10000000.f,&paragraph->layout));
        checked(paragraph->layout->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
        checked(paragraph->layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,float(metrics_.height),float(metrics_.baseline)));
        DWRITE_TEXT_METRICS total{};checked(paragraph->layout->GetMetrics(&total));
        if(!total.lineCount)throw std::runtime_error("DirectWrite returned no wrapped lines");
        if(total.lineCount>maximum_lines)throw std::length_error("Wrapped paragraph exceeds its line budget");
        std::vector<DWRITE_LINE_METRICS> lines(total.lineCount);UINT32 count=0;
        checked(paragraph->layout->GetLineMetrics(lines.data(),UINT32(lines.size()),&count));
        lines.resize(count);std::vector<NativeLineSlice> result;result.reserve(lines.size());
        std::size_t first=0;double top=0;
        for(const auto& line:lines){
            const auto last=first+std::size_t(line.length);
            result.push_back({first,last,std::make_unique<WrappedLine>(paragraph,first,last,top,line)});
            first=last;top+=line.height;
        }
        return result;
    }
};
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle style) { return std::make_unique<Line>(std::move(display),style); }
} // namespace ii::gui::detail
