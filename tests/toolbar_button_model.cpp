#include <iiGUI/ToolbarButtonModel.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace ii::gui;
    ToolbarButtonConfiguration value; value.id = std::string("open"); value.text = "Open"; value.has_icon = true;
    require(ToolbarButtonModel::validate(value) && ToolbarButtonModel::natural_width(value, 30) == 56,
            "Toolbar button defaults or natural width changed");
    auto geometry = ToolbarButtonModel::geometry(value, 56, 22, 30);
    require(geometry && geometry->icon.x == 2 && geometry->icon.width == 18 && geometry->text.x == 24 && geometry->text.width == 30,
            "Toolbar button icon and text geometry is incorrect");
    value.left_padding = 4; value.right_padding = 10; geometry = ToolbarButtonModel::geometry(value, 20, 22, 100);
    require(geometry && geometry->icon.x >= 4 && geometry->icon.width <= 6 && geometry->text.x + geometry->text.width <= 10,
            "Narrow toolbar content escaped its asymmetric padding");
    value = {}; value.palette.hover = {.1,.2,.3,1}; auto idle = ToolbarButtonModel::visual(value, {}); auto hover = ToolbarButtonModel::visual(value, {.hovered=true});
    auto focus = ToolbarButtonModel::visual(value, {.focused=true});
    require(idle.enabled && idle.background.alpha == 0 && hover.background == value.palette.hover && focus.focused,
            "Toolbar button interaction colors or focus state are incorrect");
    value.tone = ButtonTone::disabled; require(!ToolbarButtonModel::visual(value, {}).enabled, "Disabled toolbar tone remained interactive");
    value = {}; value.id = std::string{}; require(!ToolbarButtonModel::validate(value), "Empty toolbar identity was accepted");
    value = {}; value.left_padding = std::numeric_limits<double>::infinity();
    require(!ToolbarButtonModel::validate(value) && !ToolbarButtonModel::geometry(value, 20, 20, 0), "Invalid toolbar geometry was accepted");
    value = {}; value.text = std::string("bad\xff", 4); require(!ToolbarButtonModel::validate(value), "Invalid toolbar UTF-8 was accepted");
}
