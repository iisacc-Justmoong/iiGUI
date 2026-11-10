#include <iiGUI/Direct3DPaintDevice.h>
#include <iostream>

int main() {
    auto missing=ii::gui::Direct3DPaintDevice::create(nullptr);
    if(missing || missing.error().code!=ii::core::ErrorCode::invalid_argument){
        std::cerr << "A missing Direct3D swapchain was accepted\n";
        return 1;
    }
    return 0;
}
