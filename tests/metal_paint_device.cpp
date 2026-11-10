#include <iiGUI/MetalPaintDevice.h>
#include <iostream>

int main() {
    auto missing = ii::gui::MetalPaintDevice::create(nullptr);
    if (missing || missing.error().code != ii::core::ErrorCode::invalid_argument) {
        std::cerr << "A missing external Metal layer was not rejected\n";
        return 1;
    }
    return 0;
}
