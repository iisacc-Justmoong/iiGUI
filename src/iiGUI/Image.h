#pragma once
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace ii::gui {
inline constexpr std::uint32_t maximum_image_dimension=8192;
inline constexpr std::uint64_t maximum_image_pixels=16ull*1024*1024;
// Explicit opt-in for callers that own a larger raster budget. Existing APIs
// retain 16 Mi pixels; no caller can exceed 8192*8192 pixels.
struct ImageBudget final {std::uint64_t maximum_pixels=maximum_image_pixels;};
enum class ImageFit {stretch,contain,cover};
enum class ImageStatus {empty,ready};
// Immutable encoded-sRGB, straight-alpha RGBA8. Pointer identity is a stable
// render-cache key. Creation transfers the byte buffer; clients cannot mutate it.
class IIGUI_EXPORT RasterImage final {
public:
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept { return pixels_; }
private:
    RasterImage(std::uint32_t w,std::uint32_t h,std::vector<std::uint8_t> p):width_(w),height_(h),pixels_(std::move(p)) {}
    friend core::Result<std::shared_ptr<const RasterImage>> rgba_image(std::uint32_t,std::uint32_t,std::vector<std::uint8_t>);
    friend core::Result<std::shared_ptr<const RasterImage>> rgba_image(std::uint32_t,std::uint32_t,std::vector<std::uint8_t>,ImageBudget);
    std::uint32_t width_,height_;
    std::vector<std::uint8_t> pixels_;
};
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<const RasterImage>> rgba_image(
    std::uint32_t width,std::uint32_t height,std::vector<std::uint8_t> rgba);
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<const RasterImage>> rgba_image(
    std::uint32_t width,std::uint32_t height,std::vector<std::uint8_t> rgba,ImageBudget);
// Memory-only PNG, <=8 MiB and 1..2048 pixels per dimension. No file/network I/O.
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<const RasterImage>> png_image(std::span<const std::uint8_t>);
// Static native SVG assets, <=1 MiB input, 1..8192 pixels per output dimension and at most 16 Mi pixels.
// External resources and implicit system-font loading are disabled in the codec.
// Filters/animation/scripts return unsupported rather than silently wrong icons.
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<const RasterImage>> svg_image(
    std::string_view svg,std::uint32_t width=64,std::uint32_t height=64);
[[nodiscard]] IIGUI_EXPORT core::Result<std::shared_ptr<const RasterImage>> svg_image(
    std::string_view svg,std::uint32_t width,std::uint32_t height,ImageBudget);
} // namespace ii::gui
