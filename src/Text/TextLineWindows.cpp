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
class Line final: public NativeTextLine {
    ComPtr<IDWriteTextLayout> line_;
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
        ComPtr<IDWriteFactory> factory;
        checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
        const auto family = style.font_family == "Monospace" || style.font_family == "monospace" ? L"Consolas" : utf16(style.font_family.empty()?FontDatabase::application()->resolve_family(""):style.font_family);
        ComPtr<IDWriteTextFormat> format;
        auto collection=native_font_collection();
        checked(factory->CreateTextFormat(family.c_str(), collection.Get(), DWRITE_FONT_WEIGHT(style.font_weight), DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, float(style.font_size), L"", &format));
        checked(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)); checked(format->SetIncrementalTabStop(float(style.tab_stop_distance)));
        auto text = utf16(display.text());
        checked(factory->CreateTextLayout(text.data(), UINT32(text.size()), format.Get(), 1e8f, 8192, &line_));
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
        if (first == last) return {};
        UINT32 count = 0;
        auto result = line_->HitTestTextRange(UINT32(first), UINT32(last-first), 0, 0, nullptr, 0, &count);
        if (result != E_NOT_SUFFICIENT_BUFFER) checked(result);
        std::vector<DWRITE_HIT_TEST_METRICS> hits(count);
        checked(line_->HitTestTextRange(UINT32(first), UINT32(last-first), 0, 0, hits.data(), count, &count));
        std::vector<TextLineSpan> spans; spans.reserve(count);
        for (const auto& h: hits) spans.push_back({h.left,h.width}); return spans;
    }
    TextBitmap render(double x, double width, Color color, double scale) const override {
        struct Apartment final { HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); Apartment() { if (hr != RPC_E_CHANGED_MODE) checked(hr); } ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); } } apartment;
        const auto w = UINT(std::ceil(width*scale)), h = UINT(std::ceil(metrics_.height*scale));
        ComPtr<IWICImagingFactory> imaging; checked(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imaging)));
        ComPtr<IWICBitmap> bitmap; checked(imaging->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap));
        ComPtr<ID2D1Factory> factory; checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf()));
        ComPtr<ID2D1RenderTarget> drawing;
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),float(96*scale),float(96*scale));
        checked(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &drawing));
        ComPtr<ID2D1SolidColorBrush> brush; checked(drawing->CreateSolidColorBrush(D2D1::ColorF(encoded_srgb(color.red), encoded_srgb(color.green), encoded_srgb(color.blue), color.alpha), &brush));
        drawing->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE); drawing->BeginDraw(); drawing->Clear(D2D1::ColorF(0,0,0,0));
        drawing->DrawTextLayout(D2D1::Point2F(float(-x),0), line_.Get(), brush.Get(),D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT); checked(drawing->EndDraw());
        PixelBuffer pixels{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
        checked(bitmap->CopyPixels(nullptr, w*4, UINT(pixels.rgba.size()), pixels.rgba.data())); straight_rgba(pixels,true);
        return {w,h,std::move(pixels.rgba)};
    }
};
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle style) { return std::make_unique<Line>(std::move(display),style); }
} // namespace ii::gui::detail
