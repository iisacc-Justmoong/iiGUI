#include "../src/Widgets/WidgetState.h"
#include "../src/Graphics/GraphicsValidation.h"
#include <cstdio>
#include <memory>

int main() {
    using namespace ii::gui;
    using namespace ii::gui::detail;
    auto invoke = [](ii::runtime::Task task) { task(); };
    auto regular = std::make_shared<WidgetState>(invoke);
    auto embedded = std::make_shared<WidgetState>(invoke, WidgetAppearance{},
        WidgetFrameHooks{.transparent_background = true});
    const auto normalFrame = regular->drawing_snapshot({120, 80});
    const auto embeddedFrame = embedded->drawing_snapshot({120, 80});
    const auto embeddedAppearance = embedded->appearance();
    if (!normalFrame || !embeddedFrame || !embeddedAppearance || normalFrame->background.alpha != 1 ||
        embeddedFrame->background.alpha != 0 || embeddedAppearance->window.alpha != 1 ||
        valid_canvas(*embeddedFrame) || !valid_surface_canvas(*embeddedFrame)) {
        std::fprintf(stderr, "Embedded frame did not retain its underlying window or its theme colors\n");
        return 1;
    }
    regular->close(); embedded->close();
    return 0;
}
