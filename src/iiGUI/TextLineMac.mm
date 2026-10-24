#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include "TextLineNative.h"
#include "NativeText.h"

namespace ii::gui::detail {
namespace {
class Line final: public NativeTextLine {
    CTLineRef line_ = nullptr;
    TextLineMetrics metrics_;
    std::size_t length_ = 0;
public:
    Line(TextIndex display, const TextLineStyle& style): length_(display.utf16_size()) {
        @autoreleasepool {
            NSString* family = [[NSString alloc] initWithBytes:style.font_family.data() length:style.font_family.size() encoding:NSUTF8StringEncoding];
            const bool mono = [family caseInsensitiveCompare:@"Monospace"] == NSOrderedSame;
            auto weight = style.font_weight >= 700 ? NSFontWeightBold : style.font_weight >= 600 ? NSFontWeightSemibold : style.font_weight >= 500 ? NSFontWeightMedium : NSFontWeightRegular;
            NSFont* font = mono ? [NSFont monospacedSystemFontOfSize:style.font_size weight:weight] : nil;
            if (!font && family.length) {
                auto face = style.font_weight >= 700 ? @"Bold" : style.font_weight >= 600 ? @"SemiBold" : style.font_weight >= 500 ? @"Medium" : @"Regular";
                font = [NSFont fontWithName:[NSString stringWithFormat:@"%@-%@", family, face] size:style.font_size];
                if (!font) font = [NSFont fontWithName:family size:style.font_size];
            }
            if (!font) font = [NSFont systemFontOfSize:style.font_size weight:weight];
            auto paragraph = [NSMutableParagraphStyle new]; paragraph.lineBreakMode = NSLineBreakByClipping;
            paragraph.tabStops = @[]; paragraph.defaultTabInterval = style.tab_stop_distance;
            auto string = [[NSString alloc] initWithBytes:display.text().data() length:display.text().size() encoding:NSUTF8StringEncoding];
            auto attributed = [[NSAttributedString alloc] initWithString:string attributes:@{
                NSFontAttributeName:font, NSParagraphStyleAttributeName:paragraph,
                (id)kCTForegroundColorFromContextAttributeName:@YES}];
            line_ = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attributed);
            if (!line_) throw std::runtime_error("CoreText cannot shape the native paragraph");
            CGFloat ascent = 0, descent = 0, leading = 0;
            const auto width = CTLineGetTypographicBounds(line_, &ascent, &descent, &leading);
            if (!length_) { ascent = font.ascender; descent = -font.descender; leading = font.leading; }
            const auto natural = std::ceil(ascent + descent + leading);
            metrics_ = {width, style.line_height > 0 ? style.line_height : std::max(1., natural), 0};
            metrics_.baseline = (metrics_.height - ascent - descent) / 2 + ascent;
        }
    }
    ~Line() override { if (line_) CFRelease(line_); }
    TextLineMetrics metrics() const override { return metrics_; }
    double caret(std::size_t position) const override { return CTLineGetOffsetForStringIndex(line_, CFIndex(position), nullptr); }
    std::size_t hit(double x) const override {
        const auto index = CTLineGetStringIndexForPosition(line_, CGPointMake(x, 0));
        if (index == kCFNotFound) return x <= 0 ? 0 : length_;
        return std::size_t(index);
    }
    std::vector<TextLineSpan> selection(std::size_t first, std::size_t last) const override {
        std::vector<TextLineSpan> spans; if (first == last) return spans;
        auto runs = CTLineGetGlyphRuns(line_);
        for (CFIndex i = 0; i < CFArrayGetCount(runs); ++i) {
            auto run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs, i)); const auto range = CTRunGetStringRange(run);
            const auto start = std::max(first, std::size_t(range.location)), end = std::min(last, std::size_t(range.location + range.length));
            if (end <= start) continue;
            // Primary/secondary carets straddle bidi boundaries. At a run edge
            // select the caret belonging to that run's typographic interval.
            CGPoint origin{},last_origin{};const auto glyphs=CTRunGetGlyphCount(run);
            if(glyphs){CTRunGetPositions(run,CFRangeMake(0,1),&origin);CTRunGetPositions(run,CFRangeMake(glyphs-1,1),&last_origin);}
            const double left = std::min(origin.x,last_origin.x), right = left + CTRunGetTypographicBounds(run, CFRangeMake(0, 0), nullptr, nullptr, nullptr);
            const bool rtl=(CTRunGetStatus(run)&kCTRunStatusRightToLeft)!=0;
            const auto edge = [&](std::size_t p) {
                if(p==std::size_t(range.location))return rtl?right:left;
                if(p==std::size_t(range.location+range.length))return rtl?left:right;
                CGFloat secondary = 0; const auto primary = CTLineGetOffsetForStringIndex(line_, CFIndex(p), &secondary);
                const auto distance = [&](double x) { return std::abs(x - std::clamp(x, left, right)); };
                return distance(primary) <= distance(secondary) ? double(primary) : double(secondary);
            };
            const auto a = edge(start), b = edge(end); spans.push_back({std::min(a,b), std::abs(a-b)});
        }
        return spans;
    }
    TextBitmap render(double x, double width, Color color, double scale) const override {
        const auto w = std::uint32_t(std::ceil(width * scale)), h = std::uint32_t(std::ceil(metrics_.height * scale));
        PixelBuffer pixels{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
        auto space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        auto context = CGBitmapContextCreate(pixels.rgba.data(), w, h, 8, std::size_t(w)*4, space,
            CGBitmapInfo(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big); CGColorSpaceRelease(space);
        if (!context) throw std::runtime_error("CoreText cannot allocate the clipped text viewport");
        CGContextScaleCTM(context, scale, scale);
        CGContextSetRGBFillColor(context, encoded_srgb(color.red), encoded_srgb(color.green), encoded_srgb(color.blue), color.alpha);
        CGContextSetTextMatrix(context, CGAffineTransformIdentity);
        CGContextSetTextPosition(context, -x, double(h)/scale - metrics_.baseline);
        CTLineDraw(line_, context); CGContextRelease(context);
        straight_rgba(pixels, false); return {w,h,std::move(pixels.rgba)};
    }
};
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle style) { return std::make_unique<Line>(std::move(display), style); }
} // namespace ii::gui::detail
