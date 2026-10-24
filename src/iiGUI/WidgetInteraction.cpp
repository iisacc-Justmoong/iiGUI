#include "WidgetInteraction.h"
#include <cmath>
namespace ii::gui {
bool WidgetInteraction::contains(double x,double y)const noexcept {
    const auto inside=[&](Rect r){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;};
    return alive&&visible&&enabled&&!input_blocked&&std::isfinite(x)&&std::isfinite(y)&&inside(bounds)&&inside(clip);
}
} // namespace ii::gui
