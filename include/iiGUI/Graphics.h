#pragma once
#include <iiGUI/Export.h>
#include <cstdint>
#include <string>
#include <vector>

namespace ii::gui {
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
    friend bool operator==(const Panel&, const Panel&) = default;
};
enum class TextAlignment { left, center, right };
struct TextRun final {
    Rect bounds;
    std::string text;
    Color color{0,0,0,1};
    float font_size = 14;
    unsigned font_weight = 400;
    TextAlignment alignment = TextAlignment::left;
    friend bool operator==(const TextRun&, const TextRun&) = default;
};
enum class Acceleration { prefer_hardware, require_hardware };
// opengl_es3 keeps its old enum value for source compatibility; no default GLES backend remains.
enum class GraphicsApi { none, metal, direct3d11, opengl_es3, vulkan };
// Native accelerator first, then Vulkan. Linux uses Vulkan directly.
enum class Renderer { automatic, native, vulkan };
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
};
struct PixelBuffer final {
    std::uint32_t width = 0, height = 0;
    // Top-left origin, tightly packed RGBA8, sRGB, premultiplied alpha.
    std::vector<std::uint8_t> rgba;
};
// Pure validation is available before creating a native window or GPU device.
[[nodiscard]] IIGUI_EXPORT bool valid_canvas(const CanvasFrame&) noexcept;
} // namespace ii::gui
