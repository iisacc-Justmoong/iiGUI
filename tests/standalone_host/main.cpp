#include <iiGUI/Graphics.h>
#include <iiGUI/Text.h>
#include <stdexcept>

int main() {
    ii::gui::CanvasFrame frame{.viewport = {0, 0, 100, 100}};
    if (!ii::gui::valid_canvas(frame)) throw std::runtime_error("Standalone host rejected its canvas");
    auto text = ii::gui::measure_wrapped_text_layout({.bounds = {0, 0, 40, 100}, .text = "Native host"});
    if (!text || text->height <= 0) throw std::runtime_error("Standalone host did not measure text");
}
