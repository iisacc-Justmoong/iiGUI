#include <iiGUI/Host.h>
#include <iiGUI/HostFramePaintDevice.h>

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace ii::gui;
int main() {
    try {
        auto host=std::make_shared<Host>();
        auto device=std::make_shared<HostFramePaintDevice>(std::weak_ptr<Host>(host), PaintCapabilities{});
        Painter painter(device); CanvasFrame frame{.viewport={0,0,100,80}};
        auto unopened=painter.paint(frame);
        if (unopened || unopened.error().code!=ii::core::ErrorCode::closed)
            throw std::runtime_error("An unopened host accepted GPU painting");
        host.reset();
        auto retired=painter.paint(frame);
        if (retired || retired.error().code!=ii::core::ErrorCode::closed)
            throw std::runtime_error("A retired host remained owned by its paint adapter");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
