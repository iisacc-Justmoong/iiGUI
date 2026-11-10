#pragma once
#include <iiGUI/Host.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ii::gui {
struct DocumentElementBox final { std::string id; ii::gui::Rect bounds; };
struct DocumentLayout final { ii::gui::CanvasFrame frame; std::vector<DocumentElementBox> elements; };

namespace document_detail {
inline float linear(float value) {
    return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
}
template<class Color> inline ii::gui::Color color(Color value) {
    return {linear(value.red), linear(value.green), linear(value.blue), value.alpha};
}
template<class Length> inline float resolve(Length value, float basis, float automatic) {
    using Unit=decltype(value.unit);
    if (value.unit == Unit::automatic) return automatic;
    return value.unit == Unit::percent ? basis * value.value / 100 : value.value;
}
struct DocumentTypography final {
    ii::gui::Color color{0, 0, 0, 1};
    float size = 14;
    unsigned weight = 400;
    ii::gui::TextAlignment alignment = ii::gui::TextAlignment::left;
};
template<class Policy>struct DocumentLayouter final {
    using Node=typename Policy::Node;using Alignment=typename Policy::Alignment;
    using Direction=typename Policy::Direction;using Kind=typename Policy::Kind;
    using Unit=typename Policy::Unit;
    DocumentLayout result;
    [[noreturn]] void overflow(const Node& node, std::string reason) {
        throw ii::core::Error{ii::core::ErrorCode::out_of_range,
            "LVUI <" + node.tag + ">" + (node.id.empty() ? "" : " #" + node.id) + ": " + reason};
    }
    float place(const Node& node, float x, float y, float width, float available_height,
                bool root, DocumentTypography inherited) {
        const auto& style = node.style;
        if (style.color) inherited.color = color(*style.color);
        if (style.font_size) inherited.size = *style.font_size;
        if (style.font_weight) inherited.weight = *style.font_weight;
        if (style.text_align) inherited.alignment = *style.text_align == Alignment::center
            ? ii::gui::TextAlignment::center : *style.text_align == Alignment::right
            ? ii::gui::TextAlignment::right : ii::gui::TextAlignment::left;
        const bool fixed_height = root || style.height.unit != Unit::automatic;
        const float height = resolve(style.height, available_height, root ? available_height : 0);
        const auto padding = style.padding;
        const float inner_width = width - padding.left - padding.right;
        const float inner_height = (fixed_height ? height : available_height) - padding.top - padding.bottom;
        if (!std::isfinite(width) || width < 0 || width > 8192 || inner_width < 0 || inner_height < 0)
            overflow(node, "padding or size exceeds available space");
        const auto panel_index = result.frame.panels.size();
        if (style.background) result.frame.panels.push_back({});
        const auto element_index = result.elements.size();
        if (!node.id.empty()) result.elements.push_back({node.id, {}});
        float used = 0;
        if (node.kind == Kind::text) {
            if (inner_width <= 0) overflow(node, "text needs positive content width");
            ii::gui::TextRun run{{x + padding.left, y + padding.top, inner_width, 8192}, node.text,
                inherited.color, inherited.size, inherited.weight, inherited.alignment, {}};
            auto metrics = ii::gui::measure_wrapped_text_layout(run);
            if (!metrics) throw metrics.error();
            used = float(metrics->height);
            run.bounds.height = fixed_height ? inner_height : used;
            if (run.bounds.height > 0 && !run.text.empty()) result.frame.texts.push_back(std::move(run));
        } else if (!node.children.empty()) {
            const float gaps = style.gap * float(node.children.size() - 1);
            float taken = gaps;
            std::size_t automatic = 0;
            if (style.direction == Direction::row) {
                for (const auto& child : node.children) {
                    if (child.style.width.unit == Unit::automatic) ++automatic;
                    else taken += resolve(child.style.width, inner_width, 0);
                }
                if (taken > inner_width + .01f) overflow(node, "row widths and gaps exceed content width");
            }
            float advance = 0;
            for (const auto& child : node.children) {
                const bool row = style.direction == Direction::row;
                const float child_width = resolve(child.style.width, inner_width,
                    row ? (automatic ? (inner_width - taken) / float(automatic) : 0) : inner_width);
                if (child_width > inner_width + .01f) overflow(child, "width exceeds parent content");
                const float child_height = place(child, x + padding.left + (row ? advance : 0),
                    y + padding.top + (row ? 0 : advance), child_width, inner_height, false, inherited);
                if (row) { advance += child_width + style.gap; used = (std::max)(used, child_height); }
                else { advance += child_height + style.gap; used = advance - style.gap; }
            }
            if (fixed_height && used > inner_height + .01f) overflow(node, "children exceed content height");
        }
        const float actual = fixed_height ? height : used + padding.top + padding.bottom;
        if (!std::isfinite(actual) || actual < 0 || actual > 8192 || std::abs(x) > 32767 || std::abs(y) > 32767)
            overflow(node, "layout exceeds canvas geometry limits");
        const ii::gui::Rect bounds{x, y, width, actual};
        if (style.background) {
            const auto fill = color(*style.background);
            if (width > 0 && actual > 0)
                result.frame.panels[panel_index] = {bounds, fill, fill, (std::min)({style.radius, width / 2, actual / 2})};
            else result.frame.panels.erase(result.frame.panels.begin() + std::ptrdiff_t(panel_index));
        }
        if (!node.id.empty()) result.elements[element_index].bounds = bounds;
        return actual;
    }
};
} // namespace document_detail

template<class Policy> [[nodiscard]] inline ii::core::Result<DocumentLayout> layout_document(const typename Policy::Document& document,
                                                                     ii::gui::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0 || viewport.height <= 0 || viewport.width > 8192 || viewport.height > 8192)
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,
                              "LVUI viewport must be finite and within 0..8192 logical units");
    try {
        document_detail::DocumentLayouter<Policy> layout;
        layout.result.frame.viewport = {0, 0, float(viewport.width), float(viewport.height)};
        const auto& root = document.root();
        const auto width = document_detail::resolve(root.style.width, float(viewport.width), float(viewport.width));
        const auto height = layout.place(root, 0, 0, width, float(viewport.height), true, {});
        if (width > viewport.width + .01 || height > viewport.height + .01)
            return ii::core::fail(ii::core::ErrorCode::out_of_range, "LVUI root exceeds the viewport");
        if (!ii::gui::valid_canvas(layout.result.frame))
            return ii::core::fail(ii::core::ErrorCode::out_of_range, "LVUI result exceeds canvas text or geometry limits");
        return std::move(layout.result);
    } catch (const ii::core::Error& error) { return std::unexpected(error); }
      catch (const std::exception& error) { return ii::core::fail(ii::core::ErrorCode::failed, error.what()); }
}
// The view is a non-owning composition of an application document and a host policy.
template<class Policy>struct DocumentView final {
    using Document=typename Policy::Document;
    const Document& document;
    explicit DocumentView(const Document& value)noexcept:document(value){}
    DocumentView(Document&&)=delete;
};
template<class Policy>[[nodiscard]] inline ii::core::Result<void> render_view(ii::gui::Host& host,
        const DocumentView<Policy>& view,ii::gui::Renderer renderer){
    const auto window=host.snapshot();if(!window)return std::unexpected(window.error());
    auto layout=layout_document<Policy>(view.document,window->size);if(!layout)return std::unexpected(layout.error());
    layout->frame.renderer=renderer;return host.set_canvas(std::move(layout->frame));
}
} // namespace ii::gui
