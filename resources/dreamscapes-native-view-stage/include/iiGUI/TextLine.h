#pragma once
#include <iiGUI/Text.h>
#include <iiGUI/TextBoundary.h>
#include <thread>

namespace ii::gui {
struct TextLineStyle final {
    std::string font_family;
    double font_size = 13, line_height = 0, tab_stop_distance = 80;
    unsigned font_weight = 400;
    friend bool operator==(const TextLineStyle&, const TextLineStyle&) = default;
};
struct TextLineMetrics final { double width = 0, height = 0, baseline = 0; };
struct TextLineSpan final { double x = 0, width = 0; };
struct TextLineSlice;
// A shaped, unwrapped paragraph. Geometry remains double precision until the
// caller subtracts its scroll offset. Rendering allocates only the requested
// horizontal viewport, including for a line wider than CanvasFrame's limits.
// Native font services shape glyphs; no OS text control is constructed.
class IIGUI_EXPORT TextLine final {
public:
    TextLine() = default;
    [[nodiscard]] static core::Result<TextLine> create(TextIndex, TextLineStyle = {});
    [[nodiscard]] core::Result<TextLineMetrics> metrics() const;
    [[nodiscard]] core::Result<double> caret(std::size_t utf16_position) const;
    [[nodiscard]] core::Result<std::size_t> hit_test(double x) const;
    // Bidirectional selections may be disjoint. Android can merge adjacent
    // visual runs into one span; use these spans as highlight coverage.
    [[nodiscard]] core::Result<std::vector<TextLineSpan>> selection(std::size_t first, std::size_t last) const;
    [[nodiscard]] core::Result<TextBitmap> render(double x, double width, Color, double scale = 1) const;
    // Wrap the original paragraph with its native bidi and shaping context.
    // Returned ranges are UTF-16 offsets into this line, and their shaped
    // fragments expose local positions. Rewrap this paragraph, not a fragment.
    [[nodiscard]] core::Result<std::vector<TextLineSlice>> wrap(double width, std::size_t maximum_lines = 4096) const;
    [[nodiscard]] TextIndex index() const noexcept;
    // Explicit per-paragraph admission bound, independent of document size.
    static constexpr std::size_t maximum_text_bytes = 1024 * 1024;
private:
    friend class TextRasterCache;
    struct Impl;
    explicit TextLine(std::shared_ptr<const Impl>);
    [[nodiscard]] core::Result<void> ready() const;
    std::shared_ptr<const Impl> impl_;
};
struct TextLineSlice final {
    std::size_t first=0,last=0;
    TextLine line;
};
} // namespace ii::gui
