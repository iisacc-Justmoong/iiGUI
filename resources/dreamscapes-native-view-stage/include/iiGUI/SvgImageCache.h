#pragma once

#include <iiGUI/Image.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace ii::gui {
struct SvgCacheLimits final {
    std::size_t maximum_entries=128;
    std::size_t maximum_bytes=8*1024*1024;
};
struct SvgCacheStats final {std::size_t entries=0,bytes=0;};
using SvgImageDecoder=std::function<core::Result<std::shared_ptr<const RasterImage>>(std::string_view,std::uint32_t)>;

// An identity denotes immutable SVG bytes for the lifetime of that identity.
// Resolve does not perform file or network I/O. A zero limit disables retention.
class IIGUI_EXPORT SvgImageCache final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit SvgImageCache(SvgCacheLimits limits={},SvgImageDecoder decoder={});
    ~SvgImageCache();
    SvgImageCache(const SvgImageCache&)=delete;
    SvgImageCache& operator=(const SvgImageCache&)=delete;
    [[nodiscard]] core::Result<std::shared_ptr<const RasterImage>> resolve(
        std::shared_ptr<const void> identity,std::string_view svg,std::uint32_t pixels);
    [[nodiscard]] SvgCacheStats stats()const;
    void clear();
};
[[nodiscard]] IIGUI_EXPORT SvgImageCache& shared_svg_image_cache();

// Adapter suitable for asset modules that accept a decoder type.
struct CachedSvgDecoder final {
    using Image=RasterImage;
    [[nodiscard]] static core::Result<std::shared_ptr<const Image>> decode(
            std::shared_ptr<const void> identity,std::string_view svg,std::uint32_t pixels){
        return shared_svg_image_cache().resolve(std::move(identity),svg,pixels);
    }
};
} // namespace ii::gui
