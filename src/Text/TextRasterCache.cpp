#include <iiGUI/TextRasterCache.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <thread>
#include <tuple>

namespace ii::gui {
namespace {
template<class T,class F> core::Result<T> protect(F&& action) {
    try { return action(); }
    catch (const std::bad_alloc&) { return core::fail(core::ErrorCode::full,"Native text raster cache allocation failed"); }
    catch (...) { return core::fail(core::ErrorCode::failed,"Native text raster cache operation failed",std::current_exception()); }
}
bool valid(TextRasterCacheOptions o) { return o.maximum_bytes >= 1 && o.maximum_bytes <= 512*1024*1024 && o.maximum_entries >= 1 && o.maximum_entries <= 16384; }
bool valid(Color c) {
    for (float v : {c.red,c.green,c.blue,c.alpha}) if (!std::isfinite(v)||v<0||v>1) return false;
    return true;
}
}
struct TextRasterCache::Impl final {
    struct Key final {
        std::weak_ptr<const void> line;
        double x = 0, width = 0, scale = 1;
        std::array<float,3> rgb{};
    };
    struct Less final {
        bool operator()(const Key& a,const Key& b) const {
            if (a.line.owner_before(b.line)) return true;
            if (b.line.owner_before(a.line)) return false;
            return std::tie(a.x,a.width,a.scale,a.rgb)<std::tie(b.x,b.width,b.scale,b.rgb);
        }
    };
    struct Entry final { Key key; std::shared_ptr<const RasterImage> image; };
    std::thread::id owner = std::this_thread::get_id();
    TextRasterCacheOptions limits;
    TextRasterCacheStats counters;
    std::list<Entry> recent; // MRU first; one entry per immutable image.
    std::map<Key,std::list<Entry>::iterator,Less> entries;
    explicit Impl(TextRasterCacheOptions o):limits(o) {}
    core::Result<void> ready() const {
        if (owner!=std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor,"Native text raster cache requires its owner thread");
        return {};
    }
    void evict() {
        auto last=std::prev(recent.end());
        counters.bytes-=last->image->rgba().size(); entries.erase(last->key); recent.erase(last); ++counters.evictions;
    }
};
TextRasterCache::TextRasterCache(std::unique_ptr<Impl> p):impl_(std::move(p)) {}
TextRasterCache::~TextRasterCache() = default;
core::Result<std::unique_ptr<TextRasterCache>> TextRasterCache::create(TextRasterCacheOptions limits) {
    if (!valid(limits)) return core::fail(core::ErrorCode::invalid_argument,"Invalid native text raster cache limits");
    return protect<std::unique_ptr<TextRasterCache>>([&] { return std::unique_ptr<TextRasterCache>(new TextRasterCache(std::make_unique<Impl>(limits))); });
}
core::Result<TextPaint> TextRasterCache::paint(const TextLine& line,double x,double width,Color color,double scale) {
    if (auto r=impl_->ready();!r) return std::unexpected(r.error());
    if (!valid(color)||!std::isfinite(x)||std::abs(x)>1e12||!std::isfinite(width)||width<=0||width>8192||!std::isfinite(scale)||scale<=0||scale>8)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid native text raster cache request");
    auto metrics=line.metrics(); if (!metrics) return std::unexpected(metrics.error());
    const auto w=std::ceil(width*scale),h=std::ceil(metrics->height*scale);
    if(w>maximum_image_dimension||h>maximum_image_dimension||w*h>maximum_image_pixels||w*h*4>impl_->limits.maximum_bytes)
        return core::fail(core::ErrorCode::full,"Native text image exceeds its cache admission budget");
    return protect<TextPaint>([&]() -> core::Result<TextPaint> {
        Impl::Key key{line.impl_,x,width,scale,{color.red,color.green,color.blue}};
        auto found=impl_->entries.find(key);
        if(found!=impl_->entries.end()) {
            ++impl_->counters.hits; impl_->recent.splice(impl_->recent.begin(),impl_->recent,found->second);
            return TextPaint{found->second->image,color.alpha};
        }
        ++impl_->counters.misses;
        // RGB is resolved by the native glyph renderer, including color-font
        // palette layers. Opacity remains a separate compositing property.
        auto bitmap=line.render(x,width,{color.red,color.green,color.blue,1},scale);
        if(!bitmap) return std::unexpected(bitmap.error());
        auto image=rgba_image(bitmap->width,bitmap->height,std::move(bitmap->rgba));
        if(!image) return std::unexpected(image.error());
        ++impl_->counters.rasterizations; const auto bytes=(*image)->rgba().size();
        if(bytes>impl_->limits.maximum_bytes) return core::fail(core::ErrorCode::full,"Native text renderer exceeded its admitted image size");
        while(!impl_->recent.empty()&&(impl_->counters.bytes>impl_->limits.maximum_bytes-bytes||impl_->entries.size()>=impl_->limits.maximum_entries)) impl_->evict();
        impl_->recent.push_front({key,*image});
        try { impl_->entries.emplace(std::move(key),impl_->recent.begin()); }
        catch (...) { impl_->recent.pop_front(); throw; }
        impl_->counters.bytes+=bytes;
        return TextPaint{std::move(*image),color.alpha};
    });
}
core::Result<void> TextRasterCache::set_limits(TextRasterCacheOptions limits) {
    if (auto r=impl_->ready();!r) return r;
    if (!valid(limits)) return core::fail(core::ErrorCode::invalid_argument,"Invalid native text raster cache limits");
    if (limits==impl_->limits) return {};
    impl_->limits=limits;
    while(!impl_->recent.empty()&&(impl_->counters.bytes>limits.maximum_bytes||impl_->entries.size()>limits.maximum_entries)) impl_->evict();
    return {};
}
core::Result<TextRasterCacheStats> TextRasterCache::stats() const {
    if(auto r=impl_->ready();!r) return std::unexpected(r.error());
    auto result=impl_->counters;result.entries=impl_->entries.size();return result;
}
core::Result<void> TextRasterCache::clear() {
    if(auto r=impl_->ready();!r) return r;
    impl_->entries.clear();impl_->recent.clear();impl_->counters.bytes=0;return {};
}
} // namespace ii::gui
