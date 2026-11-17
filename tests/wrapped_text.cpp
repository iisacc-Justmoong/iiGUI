#include <iiGUI/Text.h>
#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <limits>
#include <memory>
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

    text.text = "MMMM";
    text.bounds.width = 420;
    text.letter_spacing = 0;
    const auto normal = ii::gui::measure_text_layout(text);
    text.letter_spacing = 3;
    const auto expanded = ii::gui::measure_text_layout(text);
    require(normal && expanded && expanded->width > normal->width + 2,
            "Positive letter spacing did not change native measurement");
    text.letter_spacing = std::numeric_limits<float>::quiet_NaN();
    require(!ii::gui::measure_text_layout(text), "Non-finite letter spacing was accepted");

    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    ii::gui::Widget label;
    auto built = state->construct([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root", {});
        if (!root) return std::unexpected(root.error());
        auto result = builder.label(*root, "label", {.text="Tracked", .letter_spacing=2});
        if (!result) return std::unexpected(result.error());
        label = *result;
        return {};
    });
    require(bool(built), "Could not construct the tracking label");
    state->commit_publication();
    auto drawing = state->drawing_snapshot({200, 80});
    require(bool(drawing) && !drawing->texts.empty() && drawing->texts[0].letter_spacing == 2,
            "Label tracking was not forwarded to the text run");
    auto invalid = label.set_label_options({.text="Tracked", .letter_spacing=std::numeric_limits<double>::infinity()});
    require(!invalid, "Invalid label tracking was accepted");
    drawing = state->drawing_snapshot({200, 80});
    require(bool(drawing) && !drawing->texts.empty() && drawing->texts[0].letter_spacing == 2,
            "Rejected label tracking changed the current value");
    state->close();
}
