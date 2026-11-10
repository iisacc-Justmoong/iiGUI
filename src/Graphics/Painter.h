#pragma once

#include <iiGUI/PainterExport.h>
#include <iiGUI/Graphics.h>
#include <iiSDKCore/Core/Result/Result.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <thread>

namespace ii::gui {

// The provider owns the native render target. iiGUI never receives a window,
// display-server, or platform widget handle through this contract.
struct PaintFrameInfo final {
    Rect viewport;
    Color background;
    double scale = 1;
    std::uint32_t pixel_width = 0, pixel_height = 0;
};
struct PaintSceneInfo final {
    std::size_t index = 0;
    Rect viewport;
    std::optional<Rect> bounds;
    bool first_is_base = false;
};
struct PaintLayerInfo final {
    std::optional<Rect> clip;
    std::optional<BackdropMaterial> backdrop;
    std::optional<BoxShadow> shadow;
    ContentTransform transform;
    bool first_is_base = false;
};
struct PaintCapabilities final {
    GraphicsApi api = GraphicsApi::none;
    bool hardware = false;
    bool native_api = false;
    bool backdrop = false;
    bool shadow = false;
    bool content_transform = false;
    bool scene_sources = false;
};

// Implemented by the application or a replaceable low-level renderer. Each
// call is synchronous on the Painter's owner thread; spans and layer inputs
// are borrowed only for the duration of that call. A provider must preserve
// the supplied ordering and layer/scene semantics. Successful begin_frame()
// starts a transaction: nothing becomes visible until present() succeeds;
// abort_frame() discards any partial work and must be safe after a failed begin.
class IIGUI_PAINTER_EXPORT PaintDevice {
public:
    virtual ~PaintDevice() = default;
    [[nodiscard]] virtual PaintCapabilities capabilities() const noexcept = 0;
    virtual core::Result<void> begin_frame(const PaintFrameInfo&) = 0;
    virtual core::Result<void> begin_scene(const PaintSceneInfo&) = 0;
    virtual core::Result<void> end_scene() = 0;
    virtual core::Result<void> begin_layer(const PaintLayerInfo&) = 0;
    virtual core::Result<void> draw_panels(std::span<const Panel>) = 0;
    virtual core::Result<void> draw_text(std::span<const TextRun>) = 0;
    virtual core::Result<void> draw_images(std::span<const ImageRun>) = 0;
    virtual core::Result<void> end_layer() = 0;
    virtual core::Result<void> present() = 0;
    virtual void abort_frame() noexcept = 0;
};

// A host-owned GPU renderer can accept the already validated scene directly.
// The frame is borrowed until this synchronous admission call returns. A
// successful return commits the submission; a failure must remain abortable.
// Backends retain their own GPU geometry and choose how to present the frame.
class IIGUI_PAINTER_EXPORT FramePaintDevice : public PaintDevice {
public:
    virtual core::Result<void> paint_frame(const CanvasFrame&, const PaintFrameInfo&, std::stop_token) = 0;
    core::Result<void> begin_frame(const PaintFrameInfo&) final;
    core::Result<void> begin_scene(const PaintSceneInfo&) final;
    core::Result<void> end_scene() final;
    core::Result<void> begin_layer(const PaintLayerInfo&) final;
    core::Result<void> draw_panels(std::span<const Panel>) final;
    core::Result<void> draw_text(std::span<const TextRun>) final;
    core::Result<void> draw_images(std::span<const ImageRun>) final;
    core::Result<void> end_layer() final;
    core::Result<void> present() final;
    void abort_frame() noexcept override {}
};

// An explicitly invoked, owner-thread painter. No timer, event loop, device,
// window, or frame is created automatically. The provider must outlive any work
// it explicitly retains; the immutable frame must not be modified during paint.
class IIGUI_PAINTER_EXPORT Painter final {
public:
    explicit Painter(std::shared_ptr<PaintDevice>);
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;
    [[nodiscard]] core::Result<void> paint(const CanvasFrame&, double scale = 1, std::stop_token = {});

private:
    std::shared_ptr<PaintDevice> device_;
    std::thread::id owner_;
    bool active_ = false;
};
} // namespace ii::gui
