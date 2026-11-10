#include <iiGUI/ListToolbarModel.h>
#include <limits>
#include <stdexcept>

namespace { void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); } }
int main() {
    ii::gui::ListBarOptions value;
    auto geometry = ii::gui::ListToolbarModel::geometry(value);
    require(geometry && geometry->width == 90 && geometry->height == 30 && geometry->slots[0].x == 8 &&
            geometry->slots[1].x == 34 && geometry->slots[2].x == 60 && geometry->icons[0].x == 2,
            "List toolbar lost its inherited default metrics");
    value.width = 20; value.height = 10; geometry = ii::gui::ListToolbarModel::geometry(value);
    require(geometry && geometry->slots[0].width >= 0 && geometry->slots[2].x + geometry->slots[2].width <= geometry->width &&
            geometry->slots[2].y + geometry->slots[2].height <= geometry->height && geometry->icons[0].width <= geometry->slots[0].width,
            "A narrow toolbar produced overflowing or negative slot geometry");
    value.icon_size = 0; require(!ii::gui::ListToolbarModel::validate(value), "Zero icon size was accepted");
    value.icon_size = 18; value.width = std::numeric_limits<double>::quiet_NaN();
    require(!ii::gui::ListToolbarModel::validate(value), "Nonfinite toolbar geometry was accepted");
    value.width = 0; value.icons[1] = std::string("bad\xff", 4);
    require(!ii::gui::ListToolbarModel::validate(value), "Invalid toolbar icon text was accepted");
}
