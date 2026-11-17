#pragma once
#include <iiGUI/TextLine.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ii::gui::detail {
// A platform may have no shaped-editor service even when label drawing is
// available. Preserve that capability boundary through the public Result.
class NativeTextUnavailable final:public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
struct NativeLineSlice;
class NativeTextLine {
public:
    virtual ~NativeTextLine() = default;
    virtual TextLineMetrics metrics() const = 0;
    virtual double caret(std::size_t) const = 0;
    virtual std::size_t hit(double) const = 0;
    virtual std::vector<TextLineSpan> selection(std::size_t, std::size_t) const = 0;
    virtual TextBitmap render(double x, double width, Color, double scale) const = 0;
    virtual std::vector<NativeLineSlice> wrap(double width, std::size_t maximum_lines) const;
};
struct NativeLineSlice final {
    std::size_t first=0,last=0;
    std::unique_ptr<NativeTextLine> line;
};
inline std::vector<NativeLineSlice> NativeTextLine::wrap(double, std::size_t) const {
    throw std::runtime_error("Native wrapped text is unavailable on this platform");
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle);
} // namespace ii::gui::detail
