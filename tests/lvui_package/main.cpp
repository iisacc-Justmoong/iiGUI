#include "Example.generated.h"
#include <iiGUI/iiGUI.h>
#if !defined(__EMSCRIPTEN__)
#include <iiGUI/VulkanPaintDevice.h>
#endif
#if defined(__APPLE__)
#include <iiGUI/MetalPaintDevice.h>
#endif
#if defined(_WIN32)
#include <iiGUI/Direct3DPaintDevice.h>
#endif

auto generated_view = &lvui_generated::Example::build;
using CreatePaintView = ii::core::Result<std::shared_ptr<ii::gui::PaintView>> (*)(
    std::shared_ptr<ii::gui::PaintDevice>, ii::gui::WidgetFactory,
    ii::gui::WidgetAppearance, ii::gui::PaintViewServices);
CreatePaintView make_paint_view = &ii::gui::PaintView::create;
#if !defined(__EMSCRIPTEN__)
auto make_vulkan_device = &ii::gui::VulkanPaintDevice::create;
#endif
#if defined(__APPLE__)
auto make_metal_device = &ii::gui::MetalPaintDevice::create;
#endif
#if defined(_WIN32)
auto make_direct3d_device = &ii::gui::Direct3DPaintDevice::create;
#endif
int main() {
    return generated_view == nullptr || make_paint_view == nullptr
#if !defined(__EMSCRIPTEN__)
        || make_vulkan_device == nullptr
#endif
#if defined(__APPLE__)
        || make_metal_device == nullptr
#endif
#if defined(_WIN32)
        || make_direct3d_device == nullptr
#endif
        ;
}
