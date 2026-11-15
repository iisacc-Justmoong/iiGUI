#pragma once
#include <algorithm>
#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicHierarchyDrag final {
    using HierarchySnapshot=typename Policy::HierarchySnapshot;
    using HierarchyIndex=typename Policy::HierarchyIndex;
    using HierarchyDragPoint=typename Policy::HierarchyDragPoint;
    using HierarchyDragUpdate=typename Policy::HierarchyDragUpdate;
    using HierarchyDragProposal=typename Policy::HierarchyDragProposal;
    using HierarchyMove=typename Policy::HierarchyMove;
    using HierarchyPlacement=typename Policy::HierarchyPlacement;
    using HierarchyError=typename Policy::HierarchyError;
    using HierarchyErrorCode=typename Policy::HierarchyErrorCode;
    using HierarchyDragViewport=typename Policy::Viewport;
    template<class T>using HierarchyResult=std::expected<T,HierarchyError>;

public:
    [[nodiscard]] static HierarchyResult<BasicHierarchyDrag> begin(std::shared_ptr<const HierarchySnapshot> snapshot,
            std::string key, HierarchyDragPoint origin, double threshold = 4, double scroll_edge = 16) {
        if (!point(origin) || !std::isfinite(threshold) || threshold < 0 || threshold > 256 ||
            !std::isfinite(scroll_edge) || scroll_edge < 0 || scroll_edge > 8192)
            return error(HierarchyErrorCode::invalid, "Invalid hierarchy drag configuration");
        auto index = HierarchyIndex::create(std::move(snapshot)); if (!index) return std::unexpected(index.error());
        const auto found = index->find(key);
        if (!found) return error(HierarchyErrorCode::not_found, "Hierarchy drag source was not found");
        const auto& entry = index->snapshot().entries[*found];
        if (!entry.enabled || !entry.draggable) return error(HierarchyErrorCode::invalid, "Hierarchy drag source cannot move");
        return BasicHierarchyDrag(std::move(*index), std::move(key), origin, threshold, scroll_edge);
    }
    [[nodiscard]] HierarchyResult<HierarchyDragUpdate> update(HierarchyDragPoint position, const HierarchyDragViewport& viewport) {
        if (!active_) return error(HierarchyErrorCode::invalid, "Hierarchy drag has ended");
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        if (!point(position) || !viewport.source || !bound(viewport.width, 0, 8192) || !bound(viewport.height, 0, 8192) ||
            !bound(viewport.offset, 0, 1e15) || !bound(viewport.row_height, 1, 8192)) {
            proposal_.reset(); return error(HierarchyErrorCode::invalid, "Invalid hierarchy drag viewport");
        }
        const auto& projection = viewport.source->projection();
        if (projection.source_revision != index_.snapshot().revision) {
            proposal_.reset(); active_ = false; return error(HierarchyErrorCode::stale, "Hierarchy changed during the drag");
        }
        if (!dragging_ && std::hypot(position.x - origin_.x, position.y - origin_.y) >= threshold_) dragging_ = true;
        proposal_.reset(); HierarchyDragUpdate result; result.dragging = dragging_;
        if (!dragging_ || viewport.width == 0 || viewport.height == 0) return result;
        const auto content_height = *viewport.source->content_extent(viewport.row_height);
        const auto edge = (std::min)(scroll_edge_, viewport.height / 2);
        if (position.y < edge && viewport.offset > 0) result.scroll_direction = -1;
        else if (position.y > viewport.height - edge && viewport.offset + viewport.height < content_height)
            result.scroll_direction = 1;
        if (position.x < 0 || position.x >= viewport.width || position.y < 0 || position.y >= viewport.height) return result;
        const auto logical = viewport.offset + position.y;
        HierarchyMove request{.key=key_, .revision=index_.snapshot().revision};
        if (logical >= content_height) request.placement = HierarchyPlacement::root_end;
        else {
            const auto row = *viewport.source->row_at(logical, viewport.row_height);
            const auto& target = projection.entries[row].entry; if (!target.enabled) return result;
            const auto fraction = (logical - *viewport.source->row_offset(row, viewport.row_height)) /
                                  *viewport.source->row_extent(row, viewport.row_height);
            request.target = target.key;
            request.placement = fraction < .25 ? HierarchyPlacement::before :
                                fraction >= .75 ? HierarchyPlacement::after : HierarchyPlacement::child;
        }
        auto plan = index_.plan(request);
        if (!plan || plan->unchanged) return result;
        proposal_ = HierarchyDragProposal{std::move(request), std::move(*plan)};
        result.proposal = proposal_; return result;
    }
    [[nodiscard]] HierarchyResult<std::optional<HierarchyDragProposal>> finish() {
        if (!active_) return error(HierarchyErrorCode::invalid, "Hierarchy drag has ended");
        active_ = false; return std::exchange(proposal_, std::nullopt);
    }
    void cancel() noexcept { active_ = false; proposal_.reset(); }
private:
    BasicHierarchyDrag(HierarchyIndex index, std::string key, HierarchyDragPoint origin, double threshold, double edge)
        : index_(std::move(index)), key_(std::move(key)), origin_(origin), threshold_(threshold), scroll_edge_(edge) {}
    [[nodiscard]] static bool point(HierarchyDragPoint p) noexcept {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e9 && std::abs(p.y) <= 1e9;
    }
    [[nodiscard]] static std::unexpected<HierarchyError> error(HierarchyErrorCode code, std::string message) {
        return std::unexpected(HierarchyError{code, std::move(message)});
    }
    HierarchyIndex index_;
    std::string key_;
    HierarchyDragPoint origin_;
    double threshold_ = 4, scroll_edge_ = 16;
    std::optional<HierarchyDragProposal> proposal_;
    bool active_ = true, dragging_ = false;
};

} // namespace ii::gui
