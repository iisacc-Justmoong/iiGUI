#include <iiGUI/Image.h>
#include <iiGUI/Graphics.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <lunasvg.h>
#include <algorithm>
#include <cmath>

namespace ii::gui {
namespace {
bool valid_image_size(std::uint32_t w,std::uint32_t h,std::size_t bytes,ImageBudget budget){
    return budget.maximum_pixels && budget.maximum_pixels<=std::uint64_t(maximum_image_dimension)*maximum_image_dimension &&
        w && h && w<=maximum_image_dimension && h<=maximum_image_dimension && std::uint64_t(w)*h<=budget.maximum_pixels &&
        bytes==std::size_t(w)*h*4;
}
// The immutable raster's ownership retains its sampling intent across copies
// of ImageRun, without changing the installed ImageRun or RasterImage layout.
struct DeviceRasterOwner final {
    void operator()(const RasterImage* image) const noexcept {delete image;}
};
}
core::Result<std::shared_ptr<const RasterImage>> rgba_image(std::uint32_t w,std::uint32_t h,std::vector<std::uint8_t> pixels) {
    return rgba_image(w,h,std::move(pixels),ImageBudget{});
}
core::Result<std::shared_ptr<const RasterImage>> rgba_image(std::uint32_t w,std::uint32_t h,std::vector<std::uint8_t> pixels,ImageBudget budget) {
    if(!valid_image_size(w,h,pixels.size(),budget))
        return core::fail(core::ErrorCode::invalid_argument,"Image exceeds its 1..8192 dimensions, explicit pixel budget, or exact RGBA8 byte count");
    return std::shared_ptr<const RasterImage>(new RasterImage(w,h,std::move(pixels)));
}
core::Result<std::shared_ptr<const RasterImage>> device_rgba_image(std::uint32_t w,std::uint32_t h,std::vector<std::uint8_t> pixels) {
    if(!valid_image_size(w,h,pixels.size(),{}))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid device raster dimensions, pixel budget, or RGBA8 byte count");
    return std::shared_ptr<const RasterImage>(new RasterImage(w,h,std::move(pixels)),DeviceRasterOwner{});
}
// Use this after applying the content transform, in the destination's device
// space. Float tolerances only absorb the conversion from pixel counts to
// logical bounds and normalized source coordinates and back.
std::optional<DeviceImagePixels> device_image_pixels(
    const ImageRun& run,Rect bounds,double scale) noexcept {
    if(!run.image||!std::get_deleter<DeviceRasterOwner>(run.image)||!std::isfinite(scale)||scale<=0)return std::nullopt;
    const int image_width=int(run.image->width()),image_height=int(run.image->height());
    const auto integral=[](double v){return std::abs(v-std::round(v))<.01;};
    const double sx=double(run.source.x)*image_width,sy=double(run.source.y)*image_height;
    const double sw=double(run.source.width)*image_width,sh=double(run.source.height)*image_height;
    if(!integral(sx)||!integral(sy)||!integral(sw)||!integral(sh))return std::nullopt;
    const auto x=std::round(sx),y=std::round(sy),w=std::round(sw),h=std::round(sh);
    if(x<0||y<0||w<=0||h<=0||x+w>image_width||y+h>image_height||
       !(std::abs(bounds.width*scale-w)<.01)||!(std::abs(bounds.height*scale-h)<.01))return std::nullopt;
    const auto dx=std::floor(bounds.x*scale+.5),dy=std::floor(bounds.y*scale+.5);
    if(!std::isfinite(dx)||!std::isfinite(dy)||std::abs(dx)>1e9||std::abs(dy)>1e9)return std::nullopt;
    return DeviceImagePixels{int(dx),int(dy),int(x),int(y),int(w),int(h)};
}

core::Result<std::shared_ptr<const RasterImage>> svg_image(std::string_view svg,std::uint32_t w,std::uint32_t h) {
    return svg_image(svg,w,h,ImageBudget{});
}
core::Result<std::shared_ptr<const RasterImage>> svg_image(std::string_view svg,std::uint32_t w,std::uint32_t h,ImageBudget budget) {
    if(!budget.maximum_pixels || budget.maximum_pixels>std::uint64_t(maximum_image_dimension)*maximum_image_dimension || svg.empty() || svg.size()>1024*1024 || !core::valid_utf8(svg) || svg.find('\0')!=svg.npos || !w || !h || w>maximum_image_dimension || h>maximum_image_dimension || std::uint64_t(w)*h>budget.maximum_pixels)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid SVG byte or output size limit");
    // Detect unsupported document elements before the codec could ignore them.
    // Quoted attributes/comments are skipped so path data is never treated as UI.
    for(std::size_t i=0;i<svg.size();) {
        if(svg[i]!='<') { ++i; continue; }
        if(svg.substr(i,4)=="<!--") { auto end=svg.find("-->",i+4); if(end==svg.npos) return core::fail(core::ErrorCode::invalid_argument,"Unclosed SVG comment"); i=end+3; continue; }
        auto begin=++i; if(i<svg.size() && svg[i]=='/') begin=++i;
        while(i<svg.size() && svg[i]!='>' && svg[i]!='/' && svg[i]!=' ' && svg[i]!='\t' && svg[i]!='\r' && svg[i]!='\n') ++i;
        auto name=svg.substr(begin,i-begin); if(auto colon=name.rfind(':');colon!=name.npos) name.remove_prefix(colon+1);
        if(name=="filter" || name=="script" || name=="foreignObject" || name.starts_with("animate") || name=="set" || name=="!DOCTYPE" || name=="!ENTITY")
            return core::fail(core::ErrorCode::unsupported,"SVG filter/active/external document constructs are not supported");
        char quote=0;
        while(i<svg.size()) { char c=svg[i++]; if(quote) { if(c==quote) quote=0; } else if(c=='\'' || c=='\"') quote=c; else if(c=='>') break; }
    }
    try {
        auto document=lunasvg::Document::loadFromData(svg.data(),svg.size());
        if(!document || !std::isfinite(document->width()) || !std::isfinite(document->height()) || document->width()<=0 || document->height()<=0)
            return core::fail(core::ErrorCode::invalid_argument,"Cannot decode a finite SVG document");
        auto bitmap=document->renderToBitmap(int(w),int(h));
        if(bitmap.isNull()) return core::fail(core::ErrorCode::failed,"SVG rasterization failed");
        bitmap.convertToRGBA(); std::vector<std::uint8_t> pixels(std::size_t(w)*h*4);
        for(std::size_t y=0;y<h;++y) std::copy_n(bitmap.data()+y*bitmap.stride(),std::size_t(w)*4,pixels.data()+y*w*4);
        return rgba_image(w,h,std::move(pixels),budget);
    } catch(...) { return core::fail(core::ErrorCode::failed,"Native SVG decoder failed",std::current_exception()); }
}
} // namespace ii::gui
