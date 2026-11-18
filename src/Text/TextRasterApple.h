#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <iiGUI/NativeText.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ii::gui::detail {

// Allocate a bounded scratch mask for native glyph coverage. Layout uses
// logical points; only this raster pass sees the higher sampling density.
inline unsigned apple_text_sample_factor(std::uint32_t width,std::uint32_t height,double scale) noexcept {
    constexpr auto scratch_limit=8u*1024u*1024u;
    const auto factor=scale>=3?1u:scale>=1.5?2u:3u;
    return std::uint64_t(width)*height*4*factor*factor<=scratch_limit?factor:1u;
}

template<class Draw>
PixelBuffer rasterize_apple_text(std::uint32_t width,std::uint32_t height,double scale,Draw&& draw) {
    const auto factor=apple_text_sample_factor(width,height,scale);
    const auto sample_width=width*factor,sample_height=height*factor;
    PixelBuffer samples{sample_width,sample_height,std::vector<std::uint8_t>(std::size_t(sample_width)*sample_height*4)};
    auto space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if(!space)throw std::runtime_error("Cannot create native text color space");
    auto context=CGBitmapContextCreate(samples.rgba.data(),sample_width,sample_height,8,std::size_t(sample_width)*4,space,
        CGBitmapInfo(kCGImageAlphaPremultipliedLast)|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if(!context)throw std::runtime_error("Cannot allocate native text coverage surface");
    const auto release=[](CGContextRef value){CGContextRelease(value);};
    const std::unique_ptr<std::remove_pointer_t<CGContextRef>,decltype(release)> owner(context,release);
    CGContextSetAllowsAntialiasing(context,true);
    CGContextSetShouldAntialias(context,true);
    // Translucent text layers can land on any background, so request alpha
    // coverage and fractional glyph positioning without LCD color fringes.
    CGContextSetAllowsFontSmoothing(context,false);
    CGContextSetShouldSmoothFonts(context,false);
    CGContextSetAllowsFontSubpixelPositioning(context,true);
    CGContextSetShouldSubpixelPositionFonts(context,true);
    CGContextSetAllowsFontSubpixelQuantization(context,false);
    CGContextSetShouldSubpixelQuantizeFonts(context,false);
    draw(context,sample_height,scale*factor);
    CGContextFlush(context);
    if(factor==1){straight_rgba(samples,false);return samples;}

    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    const auto area=factor*factor;
    for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){
        unsigned red=0,green=0,blue=0,alpha=0;
        for(unsigned j=0;j<factor;++j){
            const auto* p=samples.rgba.data()+(std::size_t(y*factor+j)*sample_width+x*factor)*4;
            for(unsigned i=0;i<factor;++i,p+=4){red+=p[0];green+=p[1];blue+=p[2];alpha+=p[3];}
        }
        const auto coverage=(alpha+area/2)/area;
        if(!coverage)continue;
        auto* p=result.rgba.data()+(std::size_t(y)*width+x)*4;
        const auto straight=[alpha](unsigned sum){return std::uint8_t(std::min(255u,(sum*255+alpha/2)/alpha));};
        p[0]=straight(red);p[1]=straight(green);p[2]=straight(blue);p[3]=std::uint8_t(coverage);
    }
    return result;
}
} // namespace ii::gui::detail
