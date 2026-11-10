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
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void good(Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
template<class T> T take(Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

struct Device final : PaintDevice {
    std::vector<std::string> text;
    int frames = 0, presented = 0, aborted = 0;
    std::weak_ptr<PaintView> close_during_paint;
    Widget mutation_target;
    bool try_mutation = false, mutation_rejected = false;
    PaintCapabilities capabilities() const noexcept override {
        return {.api=GraphicsApi::vulkan, .hardware=true, .native_api=true,
                .backdrop=true, .shadow=true, .content_transform=true, .scene_sources=true};
    }
    Result<void> begin_frame(const PaintFrameInfo&) override { ++frames; return {}; }
    Result<void> begin_scene(const PaintSceneInfo&) override { return {}; }
    Result<void> end_scene() override { return {}; }
    Result<void> begin_layer(const PaintLayerInfo&) override { return {}; }
    Result<void> draw_panels(std::span<const Panel>) override { return {}; }
    Result<void> draw_text(std::span<const TextRun> runs) override {
        for (const auto& run : runs) text.push_back(run.text);
        if (try_mutation) {
            auto mutation = mutation_target.set_text("Reentrant");
            mutation_rejected = !mutation && mutation.error().code == ii::core::ErrorCode::busy;
        }
        if (auto view = close_during_paint.lock()) good(view->close());
        return {};
    }
    Result<void> draw_images(std::span<const ImageRun>) override { return {}; }
    Result<void> end_layer() override { return {}; }
    Result<void> present() override { ++presented; return {}; }
    void abort_frame() noexcept override { ++aborted; }
};
}

int main() {
    try {
        auto device = std::make_shared<Device>();
        int published = 0, retired = 0, clicks = 0;
        auto view = take(PaintView::create(device, [&](WidgetBuilder& builder) -> Result<void> {
            auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
            auto label = builder.label(*root, "label", {.text="Before"}); if (!label) return std::unexpected(label.error());
            auto button = builder.button(*root, "button", {.text="Press"}, [&] { ++clicks; });
            if (!button) return std::unexpected(button.error());
            if (auto r = builder.on_publish([&] { ++published; }); !r) return r;
            return builder.on_retire([&] { ++retired; });
        }));
        check(published == 1 && device->frames == 0, "Publication or idle paint behavior changed");
        good(view->paint({300, 100}, 2));
        check(device->frames == 1 && device->presented == 1, "An explicit view paint failed");
        check(std::ranges::find(device->text, "Before") != device->text.end(), "Widget text did not reach the paint device");
        auto label = take(view->find("label"));
        good(label.set_text("After"));
        good(view->input({.kind=WidgetInputKind::activate, .target="button"}));
        check(clicks == 1 && device->frames == 1, "Input did not dispatch or caused an unsolicited repaint");
        device->text.clear(); device->mutation_target = label; device->try_mutation = true;
        good(view->paint({300, 100}));
        check(std::ranges::find(device->text, "After") != device->text.end(), "Changed widget content was not painted");
        check(device->mutation_rejected, "A paint device mutated the widget tree during an active frame");
        device->try_mutation = false;
        device->close_during_paint = view;
        auto canceled = view->paint({300, 100});
        check(!canceled && canceled.error().code == ii::core::ErrorCode::cancelled && device->aborted == 1 &&
              device->presented == 2 && retired == 1 && !label.snapshot(),
              "Closing an admitted paint did not abort and retire its view");
        check(!view->paint({300, 100}) && !view->input({}) && retired == 1, "A closed view accepted work");
        bool device_alive_on_retirement = false;
        auto last_device = std::make_shared<Device>();
        std::weak_ptr<Device> weak_device = last_device;
        auto owned = take(PaintView::create(last_device, [&](WidgetBuilder& builder) -> Result<void> {
            auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
            return builder.on_retire([&] { device_alive_on_retirement = !weak_device.expired(); });
        }));
        last_device.reset(); owned.reset();
        check(device_alive_on_retirement && weak_device.expired(),
              "View retirement ran after the paint device was destroyed or leaked the device");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
