#include "Example.generated.h"
#include <iiGUI/PaintView.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ii::gui;
using ii::core::Result;
namespace {
struct Device final : PaintDevice {
    std::vector<std::string> text;
    bool presented = false;
    PaintCapabilities capabilities() const noexcept override {
        return {.api=GraphicsApi::vulkan, .hardware=true, .native_api=true,
                .backdrop=true, .shadow=true, .content_transform=true, .scene_sources=true};
    }
    Result<void> begin_frame(const PaintFrameInfo&) override { return {}; }
    Result<void> begin_scene(const PaintSceneInfo&) override { return {}; }
    Result<void> end_scene() override { return {}; }
    Result<void> begin_layer(const PaintLayerInfo&) override { return {}; }
    Result<void> draw_panels(std::span<const Panel>) override { return {}; }
    Result<void> draw_text(std::span<const TextRun> runs) override {
        for (const auto& run : runs) text.push_back(run.text);
        return {};
    }
    Result<void> draw_images(std::span<const ImageRun>) override { return {}; }
    Result<void> end_layer() override { return {}; }
    Result<void> present() override { presented = true; return {}; }
    void abort_frame() noexcept override {}
};
}
int main() {
    try {
        auto device = std::make_shared<Device>();
        auto view = PaintView::create(device, [](WidgetBuilder& builder) {
            return lvui_generated::Example::build(builder);
        }, lvui_generated::Example::appearance());
        if (!view) throw std::runtime_error(view.error().message);
        if (auto result = (*view)->paint({640, 420}); !result) throw std::runtime_error(result.error().message);
        if (!device->presented || std::ranges::find(device->text, "Hello from LVUI") == device->text.end())
            throw std::runtime_error("The generated LVUI view did not reach the supplied paint device");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
