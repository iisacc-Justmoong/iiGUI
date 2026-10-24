#pragma once
#include "TextLine.h"
#include "Image.h"

namespace ii::gui {
struct TextRasterCacheOptions final {
    std::size_t maximum_bytes = 32 * 1024 * 1024, maximum_entries = 1024;
    friend bool operator==(const TextRasterCacheOptions&,const TextRasterCacheOptions&) = default;
};
struct TextRasterCacheStats final {
    std::size_t bytes = 0, entries = 0;
    std::uint64_t hits = 0, misses = 0, rasterizations = 0, evictions = 0;
};
struct TextPaint final {
    std::shared_ptr<const RasterImage> image;
    // Apply as ImageRun::opacity with no RGB tint. Native color-font pixels
    // retain their palette; opacity changes share the same opaque raster.
    float opacity = 1;
};
// Owner-thread, byte/entry-bounded LRU of clipped native text images. Keys
// include shaped-line generation, RGB, crop and scale. Weak line identities
// never extend font/layout lifetime or alias a later paragraph allocation.
class IIGUI_EXPORT TextRasterCache final {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<TextRasterCache>> create(TextRasterCacheOptions = {});
    ~TextRasterCache();
    TextRasterCache(const TextRasterCache&) = delete;
    TextRasterCache& operator=(const TextRasterCache&) = delete;
    [[nodiscard]] core::Result<TextPaint> paint(const TextLine&,double x,double width,Color,double scale = 1);
    [[nodiscard]] core::Result<void> set_limits(TextRasterCacheOptions);
    [[nodiscard]] core::Result<TextRasterCacheStats> stats() const;
    [[nodiscard]] core::Result<void> clear();
private:
    struct Impl;
    explicit TextRasterCache(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui
