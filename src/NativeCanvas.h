#pragma once
#include <iiGUI/Graphics.h>
#include <memory>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ii::gui::detail {
inline std::pair<std::uint32_t, std::uint32_t> canvas_pixels(const CanvasFrame& frame, double scale) {
    const double width = std::ceil(frame.viewport.width * scale);
    const double height = std::ceil(frame.viewport.height * scale);
    if (!std::isfinite(scale) || scale <= 0 || width < 1 || height < 1 || width > 8192 || height > 8192 ||
        std::abs(frame.viewport.x*scale)>32767 || std::abs(frame.viewport.y*scale)>32767)
        throw std::invalid_argument("Canvas physical dimensions must be 1..8192 pixels and origin within native coordinates");
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}
// Private platform boundary. No native type or handle is exposed by the SDK.
// Cocoa: parent is NSView*. Win32: parent is HWND. X11: parent is Window,
// display is Display*. All operations and destruction require the host owner.
class PlatformCanvas final {
public:
    PlatformCanvas(void* parent, void* display, Acceleration policy);
    ~PlatformCanvas();
    PlatformCanvas(const PlatformCanvas&) = delete;
    PlatformCanvas& operator=(const PlatformCanvas&) = delete;
    void draw(const CanvasFrame&, double scale, double parent_height);
    PixelBuffer capture();
    GraphicsInfo info() const;
    bool native_event(const void* event);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class NativeCanvas final {
public:
    NativeCanvas(void* parent,void* display,Acceleration policy,Renderer renderer);
    ~NativeCanvas();
    NativeCanvas(const NativeCanvas&)=delete;
    NativeCanvas& operator=(const NativeCanvas&)=delete;
    void draw(const CanvasFrame&,double scale,double parent_height);
    PixelBuffer capture();
    GraphicsInfo info() const;
    bool native_event(const void*);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
struct alignas(16) GpuPanel final {
    float bounds[4], top[4], bottom[4], radius, padding[3];
};
static_assert(sizeof(GpuPanel) == 64);
inline std::vector<GpuPanel> pack_panels(const CanvasFrame& frame) {
    std::vector<GpuPanel> result;
    result.reserve(frame.panels.size());
    for (const auto& p : frame.panels) result.push_back({
        {p.bounds.x,p.bounds.y,p.bounds.width,p.bounds.height},
        {p.top.red,p.top.green,p.top.blue,p.top.alpha},
        {p.bottom.red,p.bottom.green,p.bottom.blue,p.bottom.alpha},p.radius,{0,0,0}});
    return result;
}
} // namespace ii::gui::detail
