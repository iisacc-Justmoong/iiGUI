#include "TextLineNative.h"
#include "Image.h"
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>

namespace ii::gui {
namespace {
template<class T, class F> core::Result<T> protect(F&& f) {
    try { return f(); }
    catch (const std::bad_alloc&) { return core::fail(core::ErrorCode::full, "Native text line allocation failed"); }
    catch (const std::length_error& e) { return core::fail(core::ErrorCode::full, e.what()); }
    catch (...) { return core::fail(core::ErrorCode::failed, "Native text line operation failed", std::current_exception()); }
}
bool color_valid(Color c) {
    return std::isfinite(c.red) && c.red >= 0 && c.red <= 1 && std::isfinite(c.green) && c.green >= 0 && c.green <= 1
        && std::isfinite(c.blue) && c.blue >= 0 && c.blue <= 1 && std::isfinite(c.alpha) && c.alpha >= 0 && c.alpha <= 1;
}
}
struct TextLine::Impl final {
    std::thread::id owner = std::this_thread::get_id();
    TextIndex source;
    std::unique_ptr<detail::NativeTextLine> native;
};
TextLine::TextLine(std::shared_ptr<const Impl> p): impl_(std::move(p)) {}
core::Result<TextLine> TextLine::create(TextIndex source, TextLineStyle style) {
    if (source.text().size() > maximum_text_bytes) return core::fail(core::ErrorCode::full, "Paragraph exceeds the 1 MiB native shaping budget");
    if (source.text().find_first_of("\r\n") != source.text().npos) return core::fail(core::ErrorCode::invalid_argument, "TextLine requires one unwrapped paragraph");
    if (!std::isfinite(style.font_size) || style.font_size < 1 || style.font_size > 256 || !std::isfinite(style.line_height)
        || style.line_height < 0 || style.line_height > 1024 || !std::isfinite(style.tab_stop_distance) || style.tab_stop_distance < 1 || style.tab_stop_distance > 8192
        || style.font_weight < 400 || style.font_weight > 700 || style.font_weight % 100 || style.font_family.size() > 1024
        || !core::valid_utf8(style.font_family) || style.font_family.find('\0') != style.font_family.npos)
        return core::fail(core::ErrorCode::invalid_argument, "Invalid native text line style");
    return protect<TextLine>([&]() -> core::Result<TextLine> {
        // Control pictures preserve UTF-16 positions across native shapers,
        // including Pango, which does not admit embedded NUL text.
        std::string visible;
        for (unsigned char c: source.text()) {
            if ((c < 32 && c != '\t') || c == 127) {
                const auto cp = c == 127 ? 0x2421 : 0x2400 + c;
                visible += char(0xe0 | (cp >> 12)); visible += char(0x80 | ((cp >> 6) & 63)); visible += char(0x80 | (cp & 63));
            } else visible += char(c);
        }
        auto display = TextIndex::create(std::move(visible)); if (!display) return std::unexpected(display.error());
        auto p = std::make_shared<Impl>(); p->source = std::move(source); p->native = detail::native_text_line(std::move(*display), std::move(style));
        const auto metrics=p->native->metrics();
        if(!std::isfinite(metrics.width)||metrics.width<0||metrics.width>1e12||!std::isfinite(metrics.height)||metrics.height<=0||metrics.height>8192||!std::isfinite(metrics.baseline))
            return core::fail(core::ErrorCode::full,"Native text line geometry exceeds the supported coordinate range");
        return TextLine(std::move(p));
    });
}
core::Result<void> TextLine::ready() const {
    if (!impl_) return core::fail(core::ErrorCode::closed, "Native text line is empty");
    if (impl_->owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor, "Native text lines require their shaping thread");
    return {};
}
TextIndex TextLine::index() const noexcept { return impl_ ? impl_->source : TextIndex{}; }
core::Result<TextLineMetrics> TextLine::metrics() const {
    if (auto r = ready(); !r) return std::unexpected(r.error());
    return protect<TextLineMetrics>([&] { return impl_->native->metrics(); });
}
core::Result<double> TextLine::caret(std::size_t pos) const {
    if (auto r = ready(); !r) return std::unexpected(r.error());
    if (auto r = impl_->source.byte_for_utf16(pos); !r) return std::unexpected(r.error());
    return protect<double>([&] { return impl_->native->caret(pos); });
}
core::Result<std::size_t> TextLine::hit_test(double x) const {
    if (auto r = ready(); !r) return std::unexpected(r.error());
    if (!std::isfinite(x)) return core::fail(core::ErrorCode::invalid_argument, "Text hit position must be finite");
    x=std::clamp(x,-1e12,1e12);
    return protect<std::size_t>([&] {
        const auto& idx = impl_->source;
        auto pos = std::min(idx.utf16_size(), impl_->native->hit(x));
        // Native cluster/trailing positions differ across font services. The
        // public hit contract always resolves to an extended grapheme edge.
        if (!pos || pos == idx.utf16_size()) return pos;
        const auto first = idx.previous_grapheme(pos), last = idx.next_grapheme(first);
        if (last == pos) return pos;
        return std::abs(impl_->native->caret(first) - x) <= std::abs(impl_->native->caret(last) - x) ? first : last;
    });
}
core::Result<std::vector<TextLineSpan>> TextLine::selection(std::size_t first, std::size_t last) const {
    if (auto r = ready(); !r) return std::unexpected(r.error());
    if (first > last) return core::fail(core::ErrorCode::invalid_argument, "Text selection is reversed");
    if (auto r = impl_->source.byte_for_utf16(first); !r) return std::unexpected(r.error());
    if (auto r = impl_->source.byte_for_utf16(last); !r) return std::unexpected(r.error());
    return protect<std::vector<TextLineSpan>>([&] { return impl_->native->selection(first, last); });
}
core::Result<TextBitmap> TextLine::render(double x, double width, Color color, double scale) const {
    if (auto r = ready(); !r) return std::unexpected(r.error());
    if (!std::isfinite(x) || std::abs(x) > 1e12 || !std::isfinite(width) || width <= 0 || width > 8192
        || !std::isfinite(scale) || scale <= 0 || scale > 8 || !color_valid(color))
        return core::fail(core::ErrorCode::invalid_argument, "Invalid native text viewport or paint");
    return protect<TextBitmap>([&]() -> core::Result<TextBitmap> {
        const auto height = impl_->native->metrics().height;
        const auto w = std::ceil(width * scale), h = std::ceil(height * scale);
        if (!std::isfinite(height) || height <= 0 || w > 8192 || h > 8192 || w * h > maximum_image_pixels)
            return core::fail(core::ErrorCode::full, "Native text viewport exceeds the image budget");
        const auto alpha=color.alpha;color.alpha=1;
        auto bitmap=impl_->native->render(x,width,color,scale);
        // Native color fonts can ignore the foreground brush alpha. Apply
        // uniform opacity after rasterization so both palette and text glyphs
        // follow the same straight-RGBA contract on every backend.
        if(alpha!=1)for(std::size_t i=0;i<bitmap.rgba.size();i+=4) {
            bitmap.rgba[i+3]=std::uint8_t(std::lround(bitmap.rgba[i+3]*alpha));
            if(!bitmap.rgba[i+3])bitmap.rgba[i]=bitmap.rgba[i+1]=bitmap.rgba[i+2]=0;
        }
        return bitmap;
    });
}
} // namespace ii::gui
