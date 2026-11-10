#pragma once
#include <algorithm>
#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy> struct BasicCodeViewport final {
    using CodeDocumentView=typename Policy::DocumentView;
    using CodeDocumentErrorCode=typename Policy::ErrorCode;
    using CodeViewportRequest=typename Policy::Request;
    using CodeViewportFrame=typename Policy::Frame;
    template<class T>using CodeDocumentResult=typename Policy::template Result<T>;
// A renderer chooses a logical line/UTF-16 column window. Each request is
// bounded by both the line count and total UTF-8 bytes; Unicode scalars are
// never split at either edge. The caller may keep the originating snapshot.
[[nodiscard]] static CodeDocumentResult<CodeViewportFrame> frame(const CodeDocumentView& view, CodeViewportRequest request = {}) {
    if (!view.line_count()) return Policy::error(CodeDocumentErrorCode::invalid, "Code source has no first line");
    if (request.first_line > view.line_count() || request.line_count > 512 || request.line_units < 2 ||
        request.line_units > 65536 || request.maximum_bytes > Policy::maximum_read_bytes)
        return Policy::error(CodeDocumentErrorCode::invalid, "Invalid code viewport request");
    CodeViewportFrame result{.revision=view.revision(), .total_lines=view.line_count()};
    const auto count = (std::min)(request.line_count, result.total_lines - request.first_line);
    result.lines.reserve(count); auto remaining = request.maximum_bytes;
    for (std::size_t offset = 0; offset < count; ++offset) {
        const auto line = request.first_line + offset;
        auto bounds = Policy::line_bounds(view, line); if (!bounds) return std::unexpected(bounds.error());
        auto first = bounds->start + (std::min)(request.first_column, bounds->end - bounds->start);
        auto boundary = view.read({first, 0}, 0);
        if (!boundary && boundary.error().code == CodeDocumentErrorCode::out_of_range && first > bounds->start) {
            --first; boundary = view.read({first, 0}, 0);
        }
        if (!boundary) return std::unexpected(boundary.error());
        auto last = first + (std::min)(request.line_units, bounds->end - first);
        boundary = view.read({last, 0}, 0);
        if (!boundary && boundary.error().code == CodeDocumentErrorCode::out_of_range && last > first) {
            --last; boundary = view.read({last, 0}, 0);
        }
        if (!boundary) return std::unexpected(boundary.error());
        auto text = view.read({first, last - first}, remaining); if (!text) return std::unexpected(text.error());
        if (text->size() > remaining) return Policy::error(CodeDocumentErrorCode::invalid, "Code source exceeded the viewport read budget");
        remaining -= text->size();
        result.lines.push_back({line, bounds->start, bounds->end, first, last, std::move(*text),
            first > bounds->start, last < bounds->end});
    }
    return result;
}

};
} // namespace ii::gui
