#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#define NSFont UIFont
#define NSFontWeightBold UIFontWeightBold
#define NSFontWeightSemibold UIFontWeightSemibold
#define NSFontWeightMedium UIFontWeightMedium
#define NSFontWeightRegular UIFontWeightRegular
#else
#import <AppKit/AppKit.h>
#endif
#include "NativeText.h"
#include "FontDatabase.h"

namespace ii::gui::detail {
namespace {
NSAttributedString* attributed(const TextRun& run) {
    auto text=[[NSString alloc] initWithBytes:run.text.data() length:run.text.size() encoding:NSUTF8StringEncoding];
    auto paragraph=[NSMutableParagraphStyle new];
    paragraph.alignment=run.alignment==TextAlignment::center?NSTextAlignmentCenter:
        run.alignment==TextAlignment::right?NSTextAlignmentRight:NSTextAlignmentLeft;
    paragraph.lineBreakMode=run.elide_right?NSLineBreakByTruncatingTail:NSLineBreakByWordWrapping;
    if(run.line_height>0){paragraph.minimumLineHeight=run.line_height;paragraph.maximumLineHeight=run.line_height;}
    auto weight=run.font_weight>=700 ? NSFontWeightBold : run.font_weight>=600 ? NSFontWeightSemibold : run.font_weight>=500 ? NSFontWeightMedium : NSFontWeightRegular;
    NSFont* font=nil;
    if (!run.font_family.empty()) {
        auto family=[[NSString alloc] initWithBytes:run.font_family.data() length:run.font_family.size() encoding:NSUTF8StringEncoding];
        auto face=run.font_weight>=700 ? @"Bold" : run.font_weight>=600 ? @"SemiBold" : run.font_weight>=500 ? @"Medium" : @"Regular";
        font=[NSFont fontWithName:[NSString stringWithFormat:@"%@-%@",family,face] size:run.font_size];
        if (!font) font=[NSFont fontWithName:family size:run.font_size];
    }
    if(!font){
        const auto fallback=FontDatabase::application()->resolve_family(run.font_family);
        auto family=[[NSString alloc] initWithBytes:fallback.data() length:fallback.size() encoding:NSUTF8StringEncoding];
        auto face=run.font_weight>=700?@"Bold":run.font_weight>=600?@"SemiBold":run.font_weight>=500?@"Medium":@"Regular";
        font=[NSFont fontWithName:[NSString stringWithFormat:@"%@-%@",family,face] size:run.font_size];
        if(!font)font=[NSFont fontWithName:family size:run.font_size];
    }
    if(!font)font=[NSFont systemFontOfSize:run.font_size weight:weight];
    auto c=run.color;
    #if TARGET_OS_IOS
    auto color=[UIColor colorWithRed:encoded_srgb(c.red) green:encoded_srgb(c.green) blue:encoded_srgb(c.blue) alpha:c.alpha];
#else
    auto color=[NSColor colorWithSRGBRed:encoded_srgb(c.red) green:encoded_srgb(c.green)
        blue:encoded_srgb(c.blue) alpha:c.alpha];
#endif
    return [[NSAttributedString alloc] initWithString:text attributes:@{
        NSFontAttributeName:font, NSForegroundColorAttributeName:color, NSParagraphStyleAttributeName:paragraph,
        NSUnderlineStyleAttributeName:@(run.underline?NSUnderlineStyleSingle:NSUnderlineStyleNone)}];
}
constexpr auto options=NSStringDrawingUsesLineFragmentOrigin|NSStringDrawingUsesFontLeading;
}
float measure_text(const TextRun& run) {
    @autoreleasepool {
        if(run.elide_right)return run.line_height>0?run.line_height:std::max(run.font_size,float(std::ceil([attributed(run) size].height)));
        if(run.line_height>0){auto storage=[[NSTextStorage alloc] initWithAttributedString:attributed(run)];auto manager=[NSLayoutManager new];manager.usesFontLeading=NO;auto container=[[NSTextContainer alloc] initWithSize:CGSizeMake(std::max(1.f,run.bounds.width),CGFLOAT_MAX)];container.lineFragmentPadding=0;[manager addTextContainer:container];[storage addLayoutManager:manager];auto range=[manager glyphRangeForTextContainer:container];__block NSUInteger lines=0;[manager enumerateLineFragmentsForGlyphRange:range usingBlock:^(CGRect,CGRect,NSTextContainer*,NSRange,BOOL*){++lines;}];if(manager.extraLineFragmentTextContainer==container&&run.text.ends_with('\n'))++lines;return std::min(8192.f,float(std::max<NSUInteger>(1,lines))*run.line_height);}
#if TARGET_OS_IOS
        auto bounds=[attributed(run) boundingRectWithSize:CGSizeMake(run.bounds.width,8192) options:options context:nil];
#else
        auto bounds=[attributed(run) boundingRectWithSize:CGSizeMake(run.bounds.width,8192) options:options];
#endif
        return std::max(run.font_size,float(std::ceil(bounds.size.height)));
    }
}
float measure_text_width(const TextRun& run) {
    @autoreleasepool {
#if TARGET_OS_IOS
        auto bounds=[attributed(run) boundingRectWithSize:CGSizeMake(8192,8192) options:options context:nil];
#else
        auto bounds=[attributed(run) boundingRectWithSize:CGSizeMake(8192,8192) options:options];
#endif
        return float(std::ceil(bounds.size.width));
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
#if TARGET_OS_IOS
        UIGraphicsPushContext(context);
#else
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext=[NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
#endif
        for (const auto& run:frame.texts) {
            CGContextSaveGState(context);
            const auto r=run.bounds;
            CGContextClipToRect(context,CGRectMake(r.x,r.y,r.width,r.height));
            if(run.clip){const auto c=*run.clip;CGContextClipToRect(context,CGRectMake(c.x,c.y,c.width,c.height));}
#if TARGET_OS_IOS
            [attributed(run) drawWithRect:CGRectMake(r.x,r.y,r.width,r.height) options:run.line_height>0?NSStringDrawingUsesLineFragmentOrigin:options context:nil];
#else
            [attributed(run) drawWithRect:CGRectMake(r.x,r.y,r.width,r.height) options:run.line_height>0?NSStringDrawingUsesLineFragmentOrigin:options];
#endif
            CGContextRestoreGState(context);
        }
#if TARGET_OS_IOS
        UIGraphicsPopContext();
#else
        [NSGraphicsContext restoreGraphicsState];
#endif
        CGContextRelease(context);
        straight_rgba(result,false);
        return result;
    }
}
} // namespace ii::gui::detail
