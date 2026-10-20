#import <AppKit/AppKit.h>
#include "NativeText.h"

namespace ii::gui::detail {
namespace {
NSAttributedString* attributed(const TextRun& run) {
    auto text=[[NSString alloc] initWithBytes:run.text.data() length:run.text.size() encoding:NSUTF8StringEncoding];
    auto paragraph=[NSMutableParagraphStyle new];
    paragraph.alignment=run.alignment==TextAlignment::center?NSTextAlignmentCenter:
        run.alignment==TextAlignment::right?NSTextAlignmentRight:NSTextAlignmentLeft;
    paragraph.lineBreakMode=NSLineBreakByWordWrapping;
    auto font=[NSFont systemFontOfSize:run.font_size weight:run.font_weight==700?NSFontWeightBold:NSFontWeightRegular];
    auto c=run.color;
    auto color=[NSColor colorWithSRGBRed:encoded_srgb(c.red) green:encoded_srgb(c.green)
        blue:encoded_srgb(c.blue) alpha:c.alpha];
    return [[NSAttributedString alloc] initWithString:text attributes:@{
        NSFontAttributeName:font, NSForegroundColorAttributeName:color, NSParagraphStyleAttributeName:paragraph}];
}
constexpr auto options=NSStringDrawingUsesLineFragmentOrigin|NSStringDrawingUsesFontLeading;
}
float measure_text(const TextRun& run) {
    @autoreleasepool {
        auto bounds=[attributed(run) boundingRectWithSize:NSMakeSize(run.bounds.width,8192) options:options];
        return std::max(run.font_size,float(std::ceil(bounds.size.height)));
    }
}
PixelBuffer rasterize_text(const CanvasFrame& frame, double scale) {
    @autoreleasepool {
        const auto [width,height]=canvas_pixels(frame,scale);
        PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
        auto space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        auto context=CGBitmapContextCreate(result.rgba.data(),width,height,8,std::size_t(width)*4,space,
            CGBitmapInfo(kCGImageAlphaPremultipliedLast)|kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!context) throw std::runtime_error("Cannot allocate native LVUI text surface");
        CGContextTranslateCTM(context,0,height);
        CGContextScaleCTM(context,scale,-scale);
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext=[NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
        for (const auto& run:frame.texts) {
            CGContextSaveGState(context);
            const auto r=run.bounds;
            CGContextClipToRect(context,CGRectMake(r.x,r.y,r.width,r.height));
            [attributed(run) drawWithRect:NSMakeRect(r.x,r.y,r.width,r.height) options:options];
            CGContextRestoreGState(context);
        }
        [NSGraphicsContext restoreGraphicsState];
        CGContextRelease(context);
        straight_rgba(result,false);
        return result;
    }
}
} // namespace ii::gui::detail
