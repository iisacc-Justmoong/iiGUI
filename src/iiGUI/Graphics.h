#pragma once
#include <iiGUI/Export.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace ii::gui {
class RasterImage;
// Linear-light sRGB components. GPU attachments encode to display sRGB.
struct Color final {
    float red = 0, green = 0, blue = 0, alpha = 1;
    friend bool operator==(const Color&, const Color&) = default;
};
struct Rect final {
    float x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const Rect&, const Rect&) = default;
};
struct Panel final {
    Rect bounds;
    Color top, bottom;
    float radius = 0;
    float stroke_width=0; // Zero fills; positive values draw only the inner stroke.
    friend bool operator==(const Panel&, const Panel&) = default;
};
enum class TextAlignment { left, center, right };
struct TextRun final {
    Rect bounds;
    std::string text;
    Color color{0,0,0,1};
    float font_size = 14;
    unsigned font_weight = 400; // Regular 400, Medium 500, Semibold 600, Bold 700.
    TextAlignment alignment = TextAlignment::left;
    std::string font_family;
    float line_height=0; // Zero selects native metrics; positive values fix logical line height.
    bool elide_right=false; // One native shaped line with a trailing ellipsis.
    // Additional paint clip in the same coordinate space as bounds. Shaping
    // still uses bounds, so cropping does not reset text origin or ellipsis.
    std::optional<Rect> clip=std::nullopt;
    bool underline=false;
    friend bool operator==(const TextRun&, const TextRun&) = default;
};
enum class Acceleration { prefer_hardware, require_hardware };
// opengl_es3 keeps its old enum value for source compatibility; no default GLES backend remains.
enum class GraphicsApi { none, metal, direct3d11, opengl_es3, vulkan };
// Native accelerator first, then Vulkan. Linux uses Vulkan directly.
enum class Renderer { automatic, native, vulkan };
struct ImageGradient final {
    float height=0,middle_position=.5f;
    Color top{0,0,0,0},middle{0,0,0,.5f},bottom{0,0,0,1};
    friend bool operator==(const ImageGradient&,const ImageGradient&)=default;
};
struct ImageRun final {
    Rect bounds;
    std::shared_ptr<const RasterImage> image;
    std::optional<Color> tint; // Tint RGB replaces source RGB; source alpha is retained.
    float opacity=1;
    Rect source{0,0,1,1}; // Normalized immutable source rectangle, for center crop.
    float radius=0,border_width=0;
    Color border{0,0,0,0};
    std::optional<ImageGradient> gradient;
    friend bool operator==(const ImageRun&,const ImageRun&)=default;
};
enum class BackdropSource { content,previous_layers };
struct BackdropMaterial final {
    Rect bounds;
    float radius=0,blur_radius=0; // Blur kernel radius; native Gaussian sigma is radius/3.
    Color tint{0,0,0,0},border{0,0,0,0};
    float border_width=0;
    BackdropSource source=BackdropSource::content;
    friend bool operator==(const BackdropMaterial&,const BackdropMaterial&)=default;
};
struct BoxShadow final {
    Rect bounds;
    float radius=0,blur_radius=32;
    Color color{0,0,0,.28f};
    friend bool operator==(const BoxShadow&,const BoxShadow&)=default;
};
struct ContentTransform final {
    float origin_x=0,origin_y=0,scale=1,opacity=1;
    float offset_x=0,offset_y=0;
    [[nodiscard]] bool identity()const noexcept{return scale==1&&opacity==1&&offset_x==0&&offset_y==0;}
    friend bool operator==(const ContentTransform&,const ContentTransform&)=default;
};
struct CanvasLayer final {
    std::vector<Panel> panels;
    std::vector<TextRun> texts;
    std::vector<ImageRun> images;
    // Logical canvas coordinates. Hardware scissoring clips panels and ink
    // together without changing image sampling or rerasterizing foregrounds.
    std::optional<Rect> clip=std::nullopt;
    std::optional<BackdropMaterial> backdrop;
    std::optional<BoxShadow> shadow;
    ContentTransform content_transform{};
    bool images_before_text=false;
    friend bool operator==(const CanvasLayer&,const CanvasLayer&)=default;
};
struct CanvasFrame final {
    // Viewport is relative to the native window's logical content coordinates.
    // Panel coordinates are relative to this viewport. Later panels cover earlier ones.
    Rect viewport;
    Color background{1, 1, 1, 1};
    std::vector<Panel> panels;
    Acceleration acceleration = Acceleration::prefer_hardware;
    // Shaped native text, rectangularly clipped to each run, drawn above the panels.
    std::vector<TextRun> texts;
    Renderer renderer = Renderer::automatic;
    std::vector<ImageRun> images;
    // Each overlay draws its panels, then text/images, after the preceding layer.
    // This keeps popups above underlying glyphs as well as underlying panels.
    std::vector<CanvasLayer> overlays;
    bool images_before_text=false;
    friend bool operator==(const CanvasFrame&, const CanvasFrame&) = default;
};
struct GraphicsInfo final {
    GraphicsApi api = GraphicsApi::none;
    std::string adapter;
    // Vulkan CPU, virtual and unknown device types are conservatively false.
    bool hardware = false;
    bool srgb_attachment = false;
    std::uint32_t pixel_width = 0, pixel_height = 0;
    std::uint64_t frame_requests = 0, scene_renders = 0, presentations = 0;
    // Offscreen attachment only; excludes driver allocations and swapchain images.
    std::uint64_t cached_texture_bytes = 0;
    bool fallback = false;
    std::string fallback_reason;
    std::uint64_t foreground_rasterizations=0, foreground_texture_bytes=0;
    bool backdrop_supported=false;
    std::uint64_t backdrop_copies=0,backdrop_blurs=0,backdrop_texture_bytes=0;
    bool shadow_supported=false;
    std::uint64_t shadow_masks=0,shadow_blurs=0,shadow_texture_bytes=0;
    bool content_transform_supported=false;
};
struct PixelBuffer final {
    std::uint32_t width = 0, height = 0;
    // Top-left origin, tightly packed RGBA8, sRGB, premultiplied alpha.
    std::vector<std::uint8_t> rgba;
};
// Pure validation is available before creating a native window or GPU device.
[[nodiscard]] IIGUI_EXPORT bool valid_canvas(const CanvasFrame&) noexcept;
} // namespace ii::gui
