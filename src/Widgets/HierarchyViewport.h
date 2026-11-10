#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicHierarchyViewportSource final : public std::enable_shared_from_this<BasicHierarchyViewportSource<Policy>> {
    using HierarchyProjection=typename Policy::HierarchyProjection;
    using HierarchyViewportRequest=typename Policy::HierarchyViewportRequest;
    using HierarchyReveal=typename Policy::HierarchyReveal;
    using HierarchyError=typename Policy::HierarchyError;
    using HierarchyErrorCode=typename Policy::HierarchyErrorCode;
    using HierarchyViewportFrame=typename Policy::template Frame<BasicHierarchyViewportSource>;
    template<class T>using HierarchyResult=std::expected<T,HierarchyError>;

public:
    BasicHierarchyViewportSource(const BasicHierarchyViewportSource&) = delete;
    BasicHierarchyViewportSource& operator=(const BasicHierarchyViewportSource&) = delete;
    [[nodiscard]] static HierarchyResult<std::shared_ptr<const BasicHierarchyViewportSource>> create(
            std::shared_ptr<const HierarchyProjection> projection) {
        if (!projection || projection->entries.size() > Policy::maximum_items)
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport projection");
        std::size_t bytes = 0;
        for (std::size_t index = 0; index < projection->entries.size(); ++index) {
            const auto& row = projection->entries[index]; const auto& entry = row.entry;
            if (entry.key.empty() || entry.key.size() > 1024 || !Policy::valid_text(entry.key) ||
                !Policy::valid_text(entry.parent) || !Policy::valid_text(entry.label) || !Policy::valid_text(entry.icon_name) ||
                entry.count < -1 || row.depth > Policy::maximum_items ||
                (row.row_extent && (!std::isfinite(*row.row_extent) || *row.row_extent < 1 || *row.row_extent > 8192)) ||
                (index && row.source_index <= projection->entries[index - 1].source_index))
                return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport row");
            const auto length = entry.key.size() + entry.parent.size() + entry.label.size() + entry.icon_name.size();
            if (length > 64 * 1024 * 1024 - bytes) return error(HierarchyErrorCode::invalid, "Hierarchy viewport text exceeds its memory limit");
            bytes += length;
        }
        auto source = std::shared_ptr<BasicHierarchyViewportSource>(new BasicHierarchyViewportSource(std::make_shared<const HierarchyProjection>(*projection)));
        source->positions_.reserve(source->projection_->entries.size());
        source->override_count_.reserve(source->projection_->entries.size() + 1);
        source->override_sum_.reserve(source->projection_->entries.size() + 1);
        source->override_count_.push_back(0); source->override_sum_.push_back(0);
        for (std::size_t index = 0; index < source->projection_->entries.size(); ++index) {
            const auto& row = source->projection_->entries[index];
            if (!source->positions_.emplace(row.entry.key, index).second)
                return error(HierarchyErrorCode::invalid, "Duplicate hierarchy viewport key");
            source->override_count_.push_back(source->override_count_.back() + std::size_t(row.row_extent.has_value()));
            source->override_sum_.push_back(source->override_sum_.back() + row.row_extent.value_or(0));
        }
        return std::shared_ptr<const BasicHierarchyViewportSource>(std::move(source));
    }
    [[nodiscard]] const HierarchyProjection& projection() const noexcept { return *projection_; }
    [[nodiscard]] std::optional<std::size_t> find(std::string_view key) const {
        const auto found = positions_.find(key); return found == positions_.end() ? std::nullopt : std::optional(found->second);
    }
    [[nodiscard]] std::optional<double> content_extent(double fallback) const noexcept {
        if (!valid_fallback(fallback)) return std::nullopt;
        return offset_unchecked(projection_->entries.size(), fallback);
    }
    [[nodiscard]] std::optional<double> row_extent(std::size_t index, double fallback) const noexcept {
        if (!valid_fallback(fallback) || index >= projection_->entries.size()) return std::nullopt;
        return projection_->entries[index].row_extent.value_or(fallback);
    }
    [[nodiscard]] std::optional<double> row_offset(std::size_t index, double fallback) const noexcept {
        if (!valid_fallback(fallback) || index > projection_->entries.size()) return std::nullopt;
        return offset_unchecked(index, fallback);
    }
    [[nodiscard]] std::optional<std::size_t> row_at(double offset, double fallback) const noexcept {
        if (!valid_fallback(fallback) || !std::isfinite(offset) || offset < 0 ||
            offset >= offset_unchecked(projection_->entries.size(), fallback)) return std::nullopt;
        std::size_t first = 0, last = projection_->entries.size();
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            if (offset_unchecked(middle + 1, fallback) <= offset) first = middle + 1; else last = middle;
        }
        return first;
    }
    [[nodiscard]] HierarchyResult<HierarchyViewportFrame> frame(const HierarchyViewportRequest& request) const {
        const auto bounded = [](double value, double low, double high) { return std::isfinite(value) && value >= low && value <= high; };
        if (!bounded(request.width, 0, 8192) || !bounded(request.height, 0, 8192) ||
            !bounded(request.offset, 0, 1e15) || !bounded(request.row_height, 1, 8192) ||
            !bounded(request.anchor.within, 0, 8192) || request.overscan > Policy::maximum_items ||
            static_cast<unsigned>(request.reveal) > static_cast<unsigned>(HierarchyReveal::center) ||
            !Policy::valid_text(request.reveal_key) || !Policy::valid_text(request.anchor.key))
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy viewport request");
        HierarchyViewportFrame result; result.source = this->shared_from_this(); result.width = request.width; result.height = request.height;
        result.source_revision = projection_->source_revision; result.view_revision = projection_->view_revision; result.movement = request.movement;
        const auto count = projection_->entries.size(); result.content_height = offset_unchecked(count, request.row_height);
        auto offset = request.offset;
        if (!request.anchor.key.empty() && request.anchor.movement == request.movement && count) {
            const auto index = find(request.anchor.key).value_or((std::min)(request.anchor.index, count - 1));
            const auto begin = offset_unchecked(index, request.row_height);
            const auto end = offset_unchecked(index + 1, request.row_height);
            // Clamp the absolute position: adding nextafter(height, 0) to a
            // distant start can round back up to the next row's boundary.
            offset = (std::min)(begin + request.anchor.within, std::nextafter(end, begin));
        }
        if (!request.reveal_key.empty() && request.width > 0 && request.height > 0) if (const auto index = find(request.reveal_key)) {
            const auto height = projection_->entries[*index].row_extent.value_or(request.row_height);
            const auto begin = offset_unchecked(*index, request.row_height), end = begin + height;
            if (request.reveal == HierarchyReveal::start || height >= request.height) offset = begin;
            else if (request.reveal == HierarchyReveal::end) offset = end - request.height;
            else if (request.reveal == HierarchyReveal::center) offset = begin + (height - request.height) / 2;
            else if (begin < offset) offset = begin;
            else if (end > offset + request.height) offset = end - request.height;
            result.revealed_key = request.reveal_key;
        }
        offset = std::clamp(offset, 0., (std::max)(0., result.content_height - request.height)); result.offset = offset;
        if (!count || request.width == 0 || request.height == 0) return result;
        const auto visible_first = row_at(offset, request.row_height).value_or(count - 1);
        std::size_t visible_end = visible_first + 1, end_search = count;
        while (visible_end < end_search) {
            const auto middle = visible_end + (end_search - visible_end) / 2;
            if (offset_unchecked(middle, request.row_height) < offset + request.height) visible_end = middle + 1;
            else end_search = middle;
        }
        const auto first = visible_first > request.overscan ? visible_first - request.overscan : 0;
        const auto last = (std::min)(count, visible_end + request.overscan);
        result.rows.reserve(last - first);
        for (auto index = first; index < last; ++index)
            result.rows.push_back({index, offset_unchecked(index, request.row_height) - offset,
                                   projection_->entries[index].row_extent.value_or(request.row_height)});
        result.anchor = {projection_->entries[visible_first].entry.key, visible_first,
                         offset - offset_unchecked(visible_first, request.row_height), request.movement};
        return result;
    }
private:
    explicit BasicHierarchyViewportSource(std::shared_ptr<const HierarchyProjection> projection) : projection_(std::move(projection)) {}
    [[nodiscard]] static bool valid_fallback(double value) noexcept { return std::isfinite(value) && value >= 1 && value <= 8192; }
    [[nodiscard]] double offset_unchecked(std::size_t index, double fallback) const noexcept {
        return double(index - override_count_[index]) * fallback + override_sum_[index];
    }
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    std::shared_ptr<const HierarchyProjection> projection_;
    std::unordered_map<std::string_view, std::size_t> positions_;
    std::vector<std::size_t> override_count_;
    std::vector<double> override_sum_;
};

} // namespace ii::gui
