#include <iiGUI/Painter.h>

#include <memory>

using namespace ii::gui;
using ii::core::Result;
class Device final : public PaintDevice {
public:
    int panels = 0;
    PaintCapabilities capabilities() const noexcept override { return {}; }
    Result<void> begin_frame(const PaintFrameInfo&) override { return {}; }
    Result<void> begin_scene(const PaintSceneInfo&) override { return {}; }
    Result<void> end_scene() override { return {}; }
    Result<void> begin_layer(const PaintLayerInfo&) override { return {}; }
    Result<void> draw_panels(std::span<const Panel> values) override { panels += int(values.size()); return {}; }
    Result<void> draw_text(std::span<const TextRun>) override { return {}; }
    Result<void> draw_images(std::span<const ImageRun>) override { return {}; }
    Result<void> end_layer() override { return {}; }
    Result<void> present() override { return {}; }
    void abort_frame() noexcept override {}
};
int main() {
    auto device=std::make_shared<Device>();
    Painter painter(device);
    CanvasFrame frame{.viewport={0,0,16,16}};
    frame.panels.push_back({.bounds={0,0,10,10},.top={1,0,0,1},.bottom={1,0,0,1}});
    return painter.paint(frame) && device->panels==1 ? 0 : 1;
}
