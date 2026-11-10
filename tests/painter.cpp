#include <iiGUI/Painter.h>
#include <iiGUI/Image.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace ii::gui;
using ii::core::Result;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct Device final : PaintDevice {
    std::vector<std::string> calls;
    std::string fail_at;
    std::string throw_at;
    Painter* reenter = nullptr;
    const CanvasFrame* active_frame = nullptr;
    bool reentry_rejected = false;
    PaintFrameInfo last_frame{};
    PaintCapabilities features{.api=GraphicsApi::vulkan, .hardware=true, .native_api=true,
                               .backdrop=true, .shadow=true, .content_transform=true, .scene_sources=true};
    PaintCapabilities capabilities() const noexcept override { return features; }
    Result<void> step(std::string name) {
        calls.push_back(name);
        if (name == throw_at) throw std::runtime_error("test paint exception");
        if (name == fail_at) return ii::core::fail(ii::core::ErrorCode::failed, "test paint failure");
        return {};
    }
    Result<void> begin_frame(const PaintFrameInfo& frame) override {
        last_frame = frame;
        if (reenter) {
            auto nested = reenter->paint(*active_frame, 1);
            reentry_rejected = !nested && nested.error().code == ii::core::ErrorCode::busy;
        }
        return step("frame");
    }
    Result<void> begin_scene(const PaintSceneInfo& scene) override { return step("scene:" + std::to_string(scene.index)); }
    Result<void> end_scene() override { return step("end-scene"); }
    Result<void> begin_layer(const PaintLayerInfo& layer) override {
        return step(layer.backdrop ? "material" : "layer");
    }
    Result<void> draw_panels(std::span<const Panel> values) override { return step("panels:" + std::to_string(values.size())); }
    Result<void> draw_text(std::span<const TextRun> values) override { return step("texts:" + std::to_string(values.size())); }
    Result<void> draw_images(std::span<const ImageRun> values) override { return step("images:" + std::to_string(values.size())); }
    Result<void> end_layer() override { return step("end-layer"); }
    Result<void> present() override { return step("present"); }
    void abort_frame() noexcept override { calls.push_back("abort"); }
};

CanvasFrame fixture() {
    CanvasFrame frame;
    frame.viewport = {0, 0, 100, 80};
    frame.panels.push_back({.bounds={0,0,20,20}, .top={1,0,0,1}, .bottom={1,0,0,1}});
    frame.texts.push_back({.bounds={0,0,30,20}, .text="Hello"});
    auto image = rgba_image(1, 1, {255,255,255,255});
    check(bool(image), "Could not create the image fixture");
    frame.images.push_back({.bounds={0,0,10,10}, .image=*image});
    frame.images_before_text = true;
    CanvasScene source;
    source.viewport = frame.viewport;
    source.layers.push_back({.panels={Panel{.bounds={0,0,10,10}, .top={0,0,1,1}, .bottom={0,0,1,1}}}});
    frame.scenes.push_back(std::move(source));
    CanvasLayer overlay;
    overlay.backdrop = BackdropMaterial{.bounds={0,0,20,20}, .background_scene=0};
    overlay.texts.push_back({.bounds={0,0,20,20}, .text="Overlay"});
    frame.overlays.push_back(std::move(overlay));
    return frame;
}
}

int main() {
    try {
        auto device = std::make_shared<Device>();
        Painter painter(device);
        const auto frame = fixture();
        check(bool(painter.paint(frame, 2)), "A valid frame did not paint");
        const std::vector<std::string> expected{"frame", "scene:0", "layer", "panels:1", "end-layer", "end-scene",
            "layer", "panels:1", "images:1", "texts:1", "end-layer", "material", "texts:1", "end-layer", "present"};
        check(device->calls == expected, "Scene dependencies or paint order changed");
        check(device->last_frame.pixel_width == 200 && device->last_frame.pixel_height == 160 &&
              device->last_frame.scale == 2, "Logical-to-physical scale changed");

        device->calls.clear();
        auto invalid = frame; invalid.viewport.width = 0;
        check(!painter.paint(invalid, 1) && device->calls.empty(), "Invalid geometry reached the device");
        check(!painter.paint(frame, 1000) && device->calls.empty(), "Oversized physical target reached the device");
        auto cycle = frame;
        cycle.scenes[0].layers[0].backdrop = BackdropMaterial{.bounds={0,0,10,10}, .background_scene=0};
        check(!painter.paint(cycle, 1) && device->calls.empty(), "A cyclic scene reached the device");
        std::stop_source stopped; stopped.request_stop();
        auto canceled = painter.paint(frame, 1, stopped.get_token());
        check(!canceled && canceled.error().code == ii::core::ErrorCode::cancelled && device->calls.empty(),
              "A pre-cancelled paint reached the device");
        auto hardware = frame; hardware.acceleration = Acceleration::require_hardware;
        device->features.hardware = false;
        auto unsupported = painter.paint(hardware, 1);
        check(!unsupported && unsupported.error().code == ii::core::ErrorCode::unsupported && device->calls.empty(),
              "A hardware-only request reached a software device");
        device->features.hardware = true; device->features.backdrop = false;
        unsupported = painter.paint(frame, 1);
        check(!unsupported && unsupported.error().code == ii::core::ErrorCode::unsupported && device->calls.empty(),
              "An unsupported scene effect reached the device");
        device->features.backdrop = true;

        device->fail_at = "panels:1";
        auto failure = painter.paint(frame, 1);
        check(!failure && failure.error().code == ii::core::ErrorCode::failed && device->calls.back() == "abort",
              "A failed draw did not abort its frame");
        device->fail_at = "frame"; device->calls.clear();
        failure = painter.paint(frame, 1);
        check(!failure && device->calls == std::vector<std::string>{"frame", "abort"},
              "A failed frame start was not aborted");
        device->fail_at.clear(); device->throw_at = "texts:1"; device->calls.clear();
        failure = painter.paint(frame, 1);
        check(!failure && failure.error().cause && device->calls.back() == "abort",
              "A paint device exception was not contained and aborted");
        device->throw_at.clear();
        device->fail_at.clear(); device->calls.clear();
        device->reenter = &painter; device->active_frame = &frame;
        check(bool(painter.paint(frame, 1)) && device->reentry_rejected, "Reentrant painting was not rejected");
        bool wrong_thread = false;
        std::thread worker([&] { auto result = painter.paint(frame, 1); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
        worker.join();
        check(wrong_thread, "Painting was not confined to its owning thread");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
