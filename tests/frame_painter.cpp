#include <iiGUI/Painter.h>

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace ii::gui;
using ii::core::Result;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Device final : FramePaintDevice {
    const CanvasFrame* observed = nullptr;
    PaintFrameInfo info{};
    int calls=0, aborts=0;
    bool fail=false, throws=false;
    PaintCapabilities capabilities() const noexcept override {
        return {.api=GraphicsApi::vulkan,.hardware=true,.backdrop=true,.shadow=true,.content_transform=true,.scene_sources=true};
    }
    Result<void> paint_frame(const CanvasFrame& frame, const PaintFrameInfo& value, std::stop_token) override {
        ++calls; observed=&frame; info=value;
        if (throws) throw std::runtime_error("frame submitter exception");
        if (fail) return ii::core::fail(ii::core::ErrorCode::failed,"frame submitter failure");
        return {};
    }
    void abort_frame() noexcept override { ++aborts; }
};
}
int main() {
    try {
        auto device=std::make_shared<Device>(); Painter painter(device);
        CanvasFrame frame{.viewport={0,0,80,40}};
        frame.panels.push_back({.bounds={0,0,20,20},.top={1,0,0,1},.bottom={1,0,0,1}});
        auto result=painter.paint(frame,2);
        check(result && device->calls==1 && device->observed==&frame && device->info.pixel_width==160 &&
              device->info.pixel_height==80 && device->aborts==0,"Frame submitter lost identity, scale or commit");
        device->fail=true; result=painter.paint(frame);
        check(!result && device->aborts==1,"Failed frame submitter did not abort");
        device->fail=false; device->throws=true; result=painter.paint(frame);
        check(!result && result.error().cause && device->aborts==2,"Frame submitter exception escaped or did not abort");
        std::stop_source stop;stop.request_stop();device->throws=false;result=painter.paint(frame,1,stop.get_token());
        check(!result && result.error().code==ii::core::ErrorCode::cancelled && device->calls==3,
              "Cancelled frame reached the submitter");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
