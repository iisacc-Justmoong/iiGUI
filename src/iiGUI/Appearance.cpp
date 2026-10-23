#include "Appearance.h"
#include <cmath>

namespace ii::gui {
WidgetAppearance lvrs_appearance() {
    WidgetAppearance value;
    value.styled=true;
    value.primary={0.08120492845773697,0.5099760293960571,1,1};
    const auto mix=[](double accent) { return std::round(((20.0/255)*.98+accent*.02)*255)/255; };
    value.window={mix(value.primary.red),mix(value.primary.green),mix(value.primary.blue),1};
    // Qt.darker(primary, factor) changes HSV value: equivalent uniform RGB scale.
    value.hover={value.primary.red/1.12,value.primary.green/1.12,value.primary.blue/1.12,1};
    value.pressed={value.primary.red/1.2,value.primary.green/1.2,value.primary.blue/1.2,1};
    value.disabled_background={0.09516431391239166,0.09655113518238068,0.09793794900178909,1};
    return value;
}
} // namespace ii::gui
