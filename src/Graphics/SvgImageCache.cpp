#include "SvgImageCache.h"
#include <algorithm>
#include <exception>
#include <list>
#include <mutex>
#include <utility>

namespace ii::gui {
struct SvgImageCache::Impl final {
    struct Entry final {
        std::shared_ptr<const void> identity;
        std::uint32_t pixels;
        std::shared_ptr<const RasterImage> image;
    };
    explicit Impl(SvgCacheLimits value,SvgImageDecoder supplied):limits(value),decoder(std::move(supplied)){}
    SvgCacheLimits limits;
    SvgImageDecoder decoder;
    mutable std::mutex mutex;
    std::list<Entry> entries;
    std::size_t bytes=0;
    std::uint64_t generation=0;
};
SvgImageCache::SvgImageCache(SvgCacheLimits limits,SvgImageDecoder decoder):impl_(std::make_unique<Impl>(limits,std::move(decoder))){}
SvgImageCache::~SvgImageCache()=default;
core::Result<std::shared_ptr<const RasterImage>> SvgImageCache::resolve(
        std::shared_ptr<const void> identity,std::string_view svg,std::uint32_t pixels){
    if(!identity||!pixels||pixels>maximum_image_dimension)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid SVG cache request");
    try {
        const auto find=[&]{return std::ranges::find_if(impl_->entries,[&](const auto& entry){
            return entry.identity==identity&&entry.pixels==pixels;
        });};
        std::uint64_t generation;
        {
            std::lock_guard lock(impl_->mutex);
            if(auto found=find();found!=impl_->entries.end()){
                auto image=found->image;impl_->entries.splice(impl_->entries.begin(),impl_->entries,found);return image;
            }
            generation=impl_->generation;
        }
        auto decoded=impl_->decoder?impl_->decoder(svg,pixels):svg_image(svg,pixels,pixels);
        if(!decoded)return std::unexpected(decoded.error());
        if(!*decoded)return core::fail(core::ErrorCode::invalid_argument,"SVG decoder returned no image");
        if((*decoded)->width()!=pixels||(*decoded)->height()!=pixels)
            return core::fail(core::ErrorCode::invalid_argument,"SVG decoder returned an unexpected raster size");
        std::lock_guard lock(impl_->mutex);
        if(impl_->generation!=generation)return *decoded;
        if(auto found=find();found!=impl_->entries.end()){
            auto image=found->image;impl_->entries.splice(impl_->entries.begin(),impl_->entries,found);return image;
        }
        auto image=*decoded;const auto bytes=image->rgba().size();
        if(!impl_->limits.maximum_entries||!impl_->limits.maximum_bytes||bytes>impl_->limits.maximum_bytes)return image;
        impl_->entries.push_front({std::move(identity),pixels,image});impl_->bytes+=bytes;
        while(impl_->entries.size()>impl_->limits.maximum_entries||impl_->bytes>impl_->limits.maximum_bytes){
            impl_->bytes-=impl_->entries.back().image->rgba().size();impl_->entries.pop_back();
        }
        return image;
    }catch(...){return core::fail(core::ErrorCode::failed,"SVG cache resolution failed",std::current_exception());}
}
SvgCacheStats SvgImageCache::stats()const{
    std::lock_guard lock(impl_->mutex);return {impl_->entries.size(),impl_->bytes};
}
void SvgImageCache::clear(){std::lock_guard lock(impl_->mutex);++impl_->generation;impl_->entries.clear();impl_->bytes=0;}
SvgImageCache& shared_svg_image_cache(){static SvgImageCache cache;return cache;}
} // namespace ii::gui
