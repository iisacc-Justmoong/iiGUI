#pragma once
#include <iiGUI/Host.h>
#include <LVUI/Document.h>

namespace ii::gui {
struct ElementBox final {
    std::string id;
    Rect bounds;
};
struct ViewLayout final {
    CanvasFrame frame;
    std::vector<ElementBox> elements;
};
// Layout is independent of a window/GPU. Text measurement uses native fonts.
// The root's automatic dimensions fill the viewport. All boxes use logical units.
[[nodiscard]] IIGUI_EXPORT core::Result<ViewLayout> layout_view(const lvui::Document&, Size viewport);
} // namespace ii::gui
