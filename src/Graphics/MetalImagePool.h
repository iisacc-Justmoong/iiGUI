#pragma once

#import <Metal/Metal.h>
#include <iiGUI/Image.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ii::gui::detail {

// Confined to one Metal queue. An evicted source texture is writable only
// after the last submitted command buffer has completed, which also implies
// completion of earlier buffers on that queue.
class MetalImagePool final {
public:
    using Wanted = std::unordered_map<const RasterImage *, std::shared_ptr<const RasterImage>>;
    void prepare(id<MTLDevice> device, const Wanted &wanted, id<MTLCommandBuffer> previous) {
        std::vector<Asset> retired;
        const bool reusable = !previous || previous.status == MTLCommandBufferStatusCompleted;
        for (auto it = assets_.begin(); it != assets_.end();) {
            if (wanted.contains(it->first)) { ++it; continue; }
            if (reusable) retired.push_back(std::move(it->second));
            it = assets_.erase(it);
        }
        for (const auto &[key, image] : wanted) {
            if (assets_.contains(key)) continue;
            auto match = std::find_if(retired.begin(), retired.end(), [&](const Asset &asset) {
                return asset.texture.width == image->width() && asset.texture.height == image->height();
            });
            id<MTLTexture> texture = nil;
            if (match != retired.end()) {
                texture = match->texture;
                retired.erase(match);
                ++reuses_;
            } else {
                auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                    width:image->width() height:image->height() mipmapped:NO];
                descriptor.usage = MTLTextureUsageShaderRead;
                descriptor.storageMode = MTLStorageModeShared;
                texture = [device newTextureWithDescriptor:descriptor];
                if (!texture) throw std::runtime_error("Cannot allocate Metal image texture");
                ++allocations_;
            }
            [texture replaceRegion:MTLRegionMake2D(0,0,image->width(),image->height()) mipmapLevel:0
                        withBytes:image->rgba().data() bytesPerRow:std::size_t(image->width()) * 4];
            ++uploads_;
            assets_.emplace(key, Asset{image, texture});
        }
    }
    [[nodiscard]] id<MTLTexture> texture(const RasterImage *image) const noexcept {
        const auto it = assets_.find(image);
        return it == assets_.end() ? nil : it->second.texture;
    }
    [[nodiscard]] std::uint64_t allocations() const noexcept { return allocations_; }
    [[nodiscard]] std::uint64_t reuses() const noexcept { return reuses_; }
    [[nodiscard]] std::uint64_t uploads() const noexcept { return uploads_; }
private:
    struct Asset final { std::shared_ptr<const RasterImage> image; id<MTLTexture> texture = nil; };
    std::unordered_map<const RasterImage *, Asset> assets_;
    std::uint64_t allocations_ = 0, reuses_ = 0, uploads_ = 0;
};
} // namespace ii::gui::detail
