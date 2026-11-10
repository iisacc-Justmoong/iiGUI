#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicTableViewportSource final : public std::enable_shared_from_this<BasicTableViewportSource<Policy>> {
    using TableSnapshot=typename Policy::TableSnapshot;
    using TableIndex=typename Policy::TableIndex;
    using TableViewportConfiguration=typename Policy::TableViewportConfiguration;
    using TableViewportRequest=typename Policy::TableViewportRequest;
    using TableViewportSpan=typename Policy::TableViewportSpan;
    using TableReveal=typename Policy::TableReveal;
    using TableMerge=typename Policy::TableMerge;
    using TableError=typename Policy::TableError;
    using TableErrorCode=typename Policy::TableErrorCode;
    using TableViewportFrame=typename Policy::template Frame<BasicTableViewportSource>;
    template<class T>using TableResult=std::expected<T,TableError>;

public:
    BasicTableViewportSource(const BasicTableViewportSource&) = delete;
    BasicTableViewportSource& operator=(const BasicTableViewportSource&) = delete;
    [[nodiscard]] static TableResult<std::shared_ptr<const BasicTableViewportSource>> create(
            std::shared_ptr<const TableSnapshot> snapshot, TableViewportConfiguration configuration = {}) {
        const auto bounded = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        if (!bounded(configuration.width, 1, 8192) || !bounded(configuration.minimum_column_width, 32, 8192) ||
            !bounded(configuration.fallback_column_width, configuration.minimum_column_width, 8192))
            return error(TableErrorCode::invalid, "Invalid table viewport configuration");
        auto index = TableIndex::create(std::move(snapshot)); if (!index) return std::unexpected(index.error());
        auto result = std::shared_ptr<BasicTableViewportSource>(new BasicTableViewportSource(std::move(*index), configuration));
        const auto& data = result->index_.snapshot();
        result->row_offsets_.reserve(data.rows.size() + 1); result->row_offsets_.push_back(0);
        for (const auto& row : data.rows) result->row_offsets_.push_back(result->row_offsets_.back() + row.height);
        std::size_t flexible = 0; double fixed = 0;
        for (const auto& column : data.columns) if (column.width == 0) ++flexible; else fixed += (std::max)(column.width, configuration.minimum_column_width);
        const auto width = configuration.fit_flexible_columns && flexible ?
            std::clamp((configuration.width - fixed) / flexible, configuration.minimum_column_width, 8192.) : configuration.fallback_column_width;
        result->column_offsets_.reserve(data.columns.size() + 1); result->column_offsets_.push_back(0);
        for (const auto& column : data.columns) result->column_offsets_.push_back(result->column_offsets_.back() + (column.width ? (std::max)(column.width, configuration.minimum_column_width) : width));
        return std::shared_ptr<const BasicTableViewportSource>(std::move(result));
    }
    [[nodiscard]] const TableSnapshot& snapshot() const noexcept { return index_.snapshot(); }
    [[nodiscard]] const TableViewportConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::optional<TableMerge> merge_at(std::size_t row, std::size_t column) const noexcept {
        return index_.merge_at(row, column);
    }
    [[nodiscard]] TableResult<TableViewportFrame> frame(const TableViewportRequest& request) const {
        const auto bounded = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        if (!bounded(request.height, 0, 8192) || !bounded(request.header_height, 0, 8192) ||
            !bounded(request.x, 0, 1e15) || !bounded(request.y, 0, 1e15) ||
            !bounded(request.anchor.within_x, 0, 8192) || !bounded(request.anchor.within_y, 0, 8192) ||
            request.overscan_rows > 128 || request.overscan_columns > 128 || unsigned(request.reveal) > unsigned(TableReveal::center) ||
            !Policy::valid_text(request.anchor.row) || !Policy::valid_text(request.anchor.column) ||
            !Policy::valid_text(request.reveal_row) || !Policy::valid_text(request.reveal_column))
            return error(TableErrorCode::invalid, "Invalid table viewport request");
        TableViewportFrame result; result.source = this->shared_from_this(); result.width = configuration_.width; result.height = request.height;
        result.header_height = (std::min)(request.header_height, request.height); result.content_width = column_offsets_.back();
        result.content_height = row_offsets_.back(); result.source_revision = snapshot().revision;
        const auto body = request.height - result.header_height;
        const auto restore = [](double offset, const std::vector<double>& positions, std::optional<std::size_t> found,
                                std::size_t fallback, double within, bool enabled) {
            const auto count = positions.size() - 1; if (!enabled || !count) return offset;
            const auto i = found.value_or((std::min)(fallback, count - 1));
            return (std::min)(positions[i] + within, std::nextafter(positions[i + 1], positions[i]));
        };
        auto x = restore(request.x, column_offsets_, index_.column(request.anchor.column), request.anchor.column_index,
                         request.anchor.within_x, !request.anchor.column.empty() && request.horizontal_movement == request.anchor.horizontal_movement);
        auto y = restore(request.y, row_offsets_, index_.row(request.anchor.row), request.anchor.row_index,
                         request.anchor.within_y, !request.anchor.row.empty() && request.vertical_movement == request.anchor.vertical_movement);
        const auto reveal = [&](double offset, double extent, const std::vector<double>& positions, std::size_t first, std::size_t last) {
            const auto begin = positions[first], end = positions[last], size = end - begin;
            if (request.reveal == TableReveal::start || size >= extent) return begin;
            if (request.reveal == TableReveal::end) return end - extent;
            if (request.reveal == TableReveal::center) return begin + (size - extent) / 2;
            if (begin < offset) return begin; if (end > offset + extent) return end - extent; return offset;
        };
        const auto column = request.reveal_column.empty() ? std::nullopt : index_.column(request.reveal_column);
        const auto row = request.reveal_row.empty() ? std::nullopt : index_.row(request.reveal_row);
        if (!request.reveal_column.empty() && !column) return error(TableErrorCode::not_found, "Table reveal column was not found");
        if (!request.reveal_row.empty() && !row) return error(TableErrorCode::not_found, "Table reveal row was not found");
        const auto merged = row && column ? index_.merge_at(*row, *column) : std::nullopt;
        if (column) x = reveal(x, configuration_.width, column_offsets_, merged ? merged->column : *column,
                               merged ? merged->column + merged->column_count : *column + 1);
        if (row && body > 0) y = reveal(y, body, row_offsets_, merged ? merged->row : *row,
                                        merged ? merged->row + merged->row_count : *row + 1);
        result.x = std::clamp(x, 0., (std::max)(0., result.content_width - configuration_.width));
        result.y = std::clamp(y, 0., (std::max)(0., result.content_height - body));
        const auto spans = [](const std::vector<double>& positions, double offset, double extent, std::size_t overscan, double origin) {
            std::vector<TableViewportSpan> result; const auto count = positions.size() - 1;
            if (!count || extent <= 0) return result;
            const auto first_visible = (std::min)(count - 1, std::size_t(std::upper_bound(positions.begin(), positions.end(), offset) - positions.begin() - 1));
            const auto end_visible = (std::min)(count, std::size_t(std::lower_bound(positions.begin(), positions.end(), offset + extent) - positions.begin()));
            const auto first = first_visible > overscan ? first_visible - overscan : 0;
            const auto last = (std::min)(count, end_visible + overscan); result.reserve(last - first);
            for (auto i = first; i < last; ++i) result.push_back({i, origin + positions[i] - offset, positions[i + 1] - positions[i]});
            return result;
        };
        result.columns = spans(column_offsets_, result.x, configuration_.width, request.overscan_columns, 0);
        result.rows = spans(row_offsets_, result.y, body, request.overscan_rows, result.header_height);
        if (!snapshot().merges.empty() && !result.rows.empty() && !result.columns.empty()) {
            std::unordered_set<std::size_t> seen;
            const auto first = result.columns.front().index, last = result.columns.back().index;
            for (const auto& visible : result.rows) index_.visit_merges_in_row(visible.index, first, last, [&](const TableMerge& merge) {
                const auto key = merge.row * snapshot().columns.size() + merge.column;
                if (!seen.insert(key).second) return;
                const auto left = column_offsets_[merge.column], top = row_offsets_[merge.row];
                result.merges.push_back({merge, left - result.x, result.header_height + top - result.y,
                    column_offsets_[merge.column + merge.column_count] - left,
                    row_offsets_[merge.row + merge.row_count] - top});
            });
        }
        result.anchor.vertical_movement = request.vertical_movement; result.anchor.horizontal_movement = request.horizontal_movement;
        if (!snapshot().rows.empty() && body > 0) {
            const auto row = (std::min)(snapshot().rows.size() - 1, std::size_t(std::upper_bound(row_offsets_.begin(), row_offsets_.end(), result.y) - row_offsets_.begin() - 1));
            result.anchor.row = snapshot().rows[row].key; result.anchor.row_index = row; result.anchor.within_y = result.y - row_offsets_[row];
        }
        if (!snapshot().columns.empty()) {
            const auto column = (std::min)(snapshot().columns.size() - 1, std::size_t(std::upper_bound(column_offsets_.begin(), column_offsets_.end(), result.x) - column_offsets_.begin() - 1));
            result.anchor.column = snapshot().columns[column].key; result.anchor.column_index = column; result.anchor.within_x = result.x - column_offsets_[column];
        }
        return result;
    }
private:
    BasicTableViewportSource(TableIndex index, TableViewportConfiguration configuration) : index_(std::move(index)), configuration_(configuration) {}
    [[nodiscard]] static std::unexpected<TableError> error(TableErrorCode code, std::string message) {
        return std::unexpected(TableError{code, std::move(message)});
    }
    TableIndex index_;
    TableViewportConfiguration configuration_;
    std::vector<double> row_offsets_, column_offsets_;
};

} // namespace ii::gui
