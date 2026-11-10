#include <iiGUI/PaintView.h>

#ifdef IIGUI_HOST_DISPLAY_PIXELS_API
#error "Surface-independent views must not include the native Host API"
#endif

int main() {
    ii::gui::Size size{640,480};
    ii::gui::WindowGeometry geometry{.title="External",.width=size.width,.height=size.height};
    return geometry.width==640 && geometry.height==480 ? 0 : 1;
}
