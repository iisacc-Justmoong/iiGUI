#include "Application.generated.h"
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
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
struct Device final : PaintDevice {
    std::vector<std::string> texts;
    PaintCapabilities capabilities() const noexcept override {
        return {.api=GraphicsApi::vulkan,.hardware=true,.native_api=true,
                .backdrop=true,.shadow=true,.content_transform=true,.scene_sources=true};
    }
    Result<void> begin_frame(const PaintFrameInfo&) override { texts.clear(); return {}; }
    Result<void> begin_scene(const PaintSceneInfo&) override { return {}; }
    Result<void> end_scene() override { return {}; }
    Result<void> begin_layer(const PaintLayerInfo&) override { return {}; }
    Result<void> draw_panels(std::span<const Panel>) override { return {}; }
    Result<void> draw_text(std::span<const TextRun> values) override {
        for (const auto& value : values) texts.push_back(value.text);
        return {};
    }
    Result<void> draw_images(std::span<const ImageRun>) override { return {}; }
    Result<void> end_layer() override { return {}; }
    Result<void> present() override { return {}; }
    void abort_frame() noexcept override {}
};
}
int main() {
    try {
        using Generated = lvui_generated::Application;
        auto geometry = Generated::window_geometry();
        check(geometry.title == "External application" && geometry.width == 800 && geometry.height == 480 &&
              geometry.minimum_width == 320 && geometry.minimum_height == 220,
              "Generated initial window geometry was lost at the external host boundary");
        auto application = Generated::make_application_owner();
        good(application->activate());
        auto device = std::make_shared<Device>();
        ii::runtime::EventLoop events(16);
        int invalidated = 0;
        auto view = take(PaintView::create(device, [application](WidgetBuilder& builder) {
            return Generated::build_with_application(builder, {}, application);
        }, Generated::appearance(), {.events=&events, .invalidated=[&]{++invalidated;}}));
        good(view->paint({320,160}));
        check(std::ranges::find(device->texts, "0") != device->texts.end(), "Initial application state was not painted");
        (void)events.poll();
        good(view->input({.kind=WidgetInputKind::activate,.target="increment"}));
        (void)events.poll();
        check(invalidated > 0 && take(view->needs_paint()), "Binding change did not invalidate the external view");
        good(view->paint({320,160}));
        check(std::ranges::find(device->texts, "1") != device->texts.end(), "Input did not update the generated binding");
        good(view->close());
        good(application->retire());
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
