#pragma once
#include <iiGUI/TextLine.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ii::gui::detail {
class NativeTextLine {
public:
    virtual ~NativeTextLine() = default;
    virtual TextLineMetrics metrics() const = 0;
    virtual double caret(std::size_t) const = 0;
    virtual std::size_t hit(double) const = 0;
    virtual std::vector<TextLineSpan> selection(std::size_t, std::size_t) const = 0;
    virtual TextBitmap render(double x, double width, Color, double scale) const = 0;
};
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle);
} // namespace ii::gui::detail
