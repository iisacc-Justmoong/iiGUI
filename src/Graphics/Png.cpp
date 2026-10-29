#include <iiGUI/Image.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace {
// The pinned decoder can expand a zlib stream beyond its declared PNG dimensions.
// Bound every codec allocation, including reallocations, before that can happen.
struct PngBudget final {std::size_t used=0;bool exhausted=false;};
thread_local PngBudget* current_png_budget=nullptr;
struct alignas(std::max_align_t) Allocation final {std::size_t size;};
constexpr std::size_t codec_limit=96*1024*1024;
void* png_allocate(std::size_t n) {
    if(!current_png_budget || current_png_budget->used>codec_limit-sizeof(Allocation) || n>codec_limit-sizeof(Allocation)-current_png_budget->used){if(current_png_budget)current_png_budget->exhausted=true;return nullptr;}
    auto p=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+n));if(!p)return nullptr;p->size=n;current_png_budget->used+=n+sizeof(Allocation);return p+1;
}
void png_free(void* p) {
    if(!p)return;auto a=static_cast<Allocation*>(p)-1;if(current_png_budget)current_png_budget->used-=a->size+sizeof(Allocation);std::free(a);
}
void* png_reallocate(void* p,std::size_t n) {
    if(!p)return png_allocate(n);auto a=static_cast<Allocation*>(p)-1;auto next=png_allocate(n);if(!next)return nullptr;
    std::memcpy(next,p,std::min(a->size,n));png_free(p);return next;
}
}
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 2048
#define STBI_MALLOC(n) png_allocate(n)
#define STBI_REALLOC_SIZED(p,old_size,new_size) png_reallocate(p,new_size)
#define STBI_FREE(p) png_free(p)
#include "plutovg-stb-image.h"

namespace ii::gui {
core::Result<std::shared_ptr<const RasterImage>> png_image(std::span<const std::uint8_t> data) {
    constexpr std::uint8_t magic[]{137,80,78,71,13,10,26,10};
    if(data.size()<33 || data.size()>8*1024*1024 || !std::equal(std::begin(magic),std::end(magic),data.begin()) || std::memcmp(data.data()+12,"IHDR",4)!=0)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid bounded PNG data");
    const auto integer=[&](std::size_t at){return (std::uint32_t(data[at])<<24)|(std::uint32_t(data[at+1])<<16)|(std::uint32_t(data[at+2])<<8)|data[at+3];};
    const auto w=integer(16),h=integer(20);if(integer(8)!=13 || !w || !h || w>2048 || h>2048)return core::fail(core::ErrorCode::invalid_argument,"PNG dimensions must be 1..2048");
    if(current_png_budget)return core::fail(core::ErrorCode::busy,"Recursive PNG decode is not supported");
    PngBudget budget;current_png_budget=&budget;struct Reset final {~Reset(){current_png_budget=nullptr;}}reset;
    int width=0,height=0,channels=0;
    auto decoded=std::unique_ptr<stbi_uc,decltype(&stbi_image_free)>(stbi_load_from_memory(data.data(),int(data.size()),&width,&height,&channels,STBI_rgb_alpha),stbi_image_free);
    if(!decoded)return core::fail(budget.exhausted?core::ErrorCode::out_of_range:core::ErrorCode::invalid_argument,budget.exhausted?"PNG codec allocation budget exceeded":"PNG decoder rejected image");
    if(width!=int(w) || height!=int(h))return core::fail(core::ErrorCode::invalid_argument,"PNG dimensions changed during decode");
    return rgba_image(w,h,std::vector<std::uint8_t>(decoded.get(),decoded.get()+std::size_t(w)*h*4));
}
} // namespace ii::gui
