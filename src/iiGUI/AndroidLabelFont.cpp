#include "AndroidLabelFont.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <plutovg-stb-truetype.h>

namespace ii::gui::detail {
struct AndroidLabelFont::Impl final {
    std::vector<std::uint8_t> bytes;
    stbtt_fontinfo font{};
    explicit Impl(std::vector<std::uint8_t> data,unsigned index):bytes(std::move(data)){
        if(bytes.size()<12||bytes.size()>16*1024*1024)throw std::invalid_argument("Invalid pinned label font size");
        if(index>4095)throw std::invalid_argument("Font collection index exceeds the supported bound");
        const auto offset=stbtt_GetFontOffsetForIndex(bytes.data(),int(index));
        if(offset<0||std::size_t(offset)>bytes.size()-12||!stbtt_InitFont(&font,bytes.data(),offset))
            throw std::invalid_argument("Cannot initialize the pinned native label font");
    }
    float scale(float pixels)const{if(!std::isfinite(pixels)||pixels<=0||pixels>2048)throw std::invalid_argument("Native label font size must be in (0,2048]");return stbtt_ScaleForPixelHeight(&font,pixels);}
};
AndroidLabelFont::AndroidLabelFont(std::vector<std::uint8_t> bytes,unsigned index):impl_(std::make_unique<Impl>(std::move(bytes),index)){}
AndroidLabelFont::~AndroidLabelFont()=default;
float AndroidLabelFont::width(float pixels,std::string_view text)const{
    const auto scale=impl_->scale(pixels);float width=0;unsigned previous=0;
    for(unsigned char c:text){if(c<32||c>126)throw std::invalid_argument("The minimal Android label font supports printable ASCII");int advance=0;stbtt_GetCodepointHMetrics(&impl_->font,c,&advance,nullptr);if(previous)width+=stbtt_GetCodepointKernAdvance(&impl_->font,int(previous),c)*scale;width+=advance*scale;previous=c;}return width;
}
float AndroidLabelFont::ascent(float pixels)const{const auto scale=impl_->scale(pixels);int ascent=0;stbtt_GetFontVMetrics(&impl_->font,&ascent,nullptr,nullptr);return ascent*scale;}
float AndroidLabelFont::height(float pixels)const{const auto scale=impl_->scale(pixels);int ascent=0,descent=0,gap=0;stbtt_GetFontVMetrics(&impl_->font,&ascent,&descent,&gap);return (ascent-descent+gap)*scale;}
float AndroidLabelFont::kerning(float pixels,unsigned first,unsigned second)const{return stbtt_GetCodepointKernAdvance(&impl_->font,int(first),int(second))*impl_->scale(pixels);}
LabelGlyph AndroidLabelFont::glyph(float pixels,unsigned codepoint)const{
    if(codepoint<32||codepoint>126)throw std::invalid_argument("The minimal Android label glyph supports printable ASCII");
    const auto scale=impl_->scale(pixels);LabelGlyph glyph;int advance=0;stbtt_GetCodepointHMetrics(&impl_->font,int(codepoint),&advance,nullptr);glyph.advance=advance*scale;
    auto* bitmap=stbtt_GetCodepointBitmap(&impl_->font,scale,scale,int(codepoint),&glyph.width,&glyph.height,&glyph.x,&glyph.y);
    struct Release final{unsigned char* p;~Release(){stbtt_FreeBitmap(p,nullptr);}} release{bitmap};
    if(glyph.width<0||glyph.height<0||glyph.width>4096||glyph.height>4096)throw std::runtime_error("Native glyph exceeded its pixel budget");
    const auto size=std::size_t(glyph.width)*glyph.height;if(size&&!bitmap)throw std::bad_alloc();if(size)glyph.coverage.assign(bitmap,bitmap+size);return glyph;
}
}
