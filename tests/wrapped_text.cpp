#include <iiGUI/Text.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}
int main() {
    ii::gui::TextRun text{.bounds = {0, 0, 420, 8192},
        .text = "Wrapped native text must measure the width supplied by the caller.", .font_size = 18};
    const auto wide = ii::gui::measure_wrapped_text_layout(text);
    text.bounds.width = 80;
    const auto narrow = ii::gui::measure_wrapped_text_layout(text);
    require(wide && narrow && narrow->height > wide->height && wide->height > 0,
            "Width-constrained text measurement did not wrap");
    text.bounds.width = 0;
    require(!ii::gui::measure_wrapped_text_layout(text), "Zero text width was accepted");
    text.bounds.width = std::numeric_limits<float>::infinity();
    require(!ii::gui::measure_wrapped_text_layout(text), "Non-finite text width was accepted");
}
