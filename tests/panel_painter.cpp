#include <iiGUI/PanelPainter.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}
int main() {
    using namespace ii::gui;
    PanelPainter painter;
    PanelColors colors{.background = WidgetColor{.5, .25, .75, .8},
                       .border = WidgetColor{1, 1, 1, 1}, .width = 2, .radius = 4};
    good(painter.set_colors(colors));
    WidgetPaintState paint{.size = {120, 60}};
    auto first = take(painter(paint));
    require(first->panels.size() == 2 && first->panels[0].bounds == Rect{0, 0, 120, 60} &&
            first->panels[0].radius == 4 && first->panels[1].stroke_width == 2,
            "Panel geometry changed");
    require(std::abs(first->panels[0].top.red - .21404114) < .00001 &&
            std::abs(first->panels[0].top.alpha - .8) < .00001,
            "Panel did not convert encoded sRGB to linear light");
    require(take(painter(paint)) == first, "An unchanged panel was redrawn");
    auto invalid = colors; invalid.border.reset();
    auto bad = painter.set_colors(invalid);
    require(!bad && bad.error().code == ii::core::ErrorCode::invalid_argument && take(painter(paint)) == first,
            "Invalid panel replaced its previous drawing");
    invalid = colors; invalid.background->red = std::numeric_limits<double>::quiet_NaN();
    require(!painter.set_colors(invalid), "Non-finite panel color was accepted");
    auto next = colors; next.background = WidgetColor{.2, .4, .6, 1}; good(painter.set_colors(next));
    auto changed = take(painter(paint)); require(changed != first, "Changed panel reused stale drawing");
    paint.size.width = 200; require(take(painter(paint)) != changed, "Resized panel reused stale drawing");
    paint.size.width = -1; require(!painter(paint), "Invalid panel extent was accepted");
    bool rejected = false; std::thread worker([&] {
        auto result = painter.set_colors(colors);
        rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
    }); worker.join(); require(rejected, "Panel painter accepted another thread");
}
