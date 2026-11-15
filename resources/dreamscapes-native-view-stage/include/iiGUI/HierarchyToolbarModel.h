#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
namespace ii::gui {
template<class Policy>class BasicHierarchyToolbar final {
    using HierarchyToolbarItem=typename Policy::Item;using ToolbarButtonId=typename Policy::Id;using HierarchyToolbarSnapshot=typename Policy::Snapshot;
    using HierarchyToolbarSelection=typename Policy::Selection;using HierarchyToolbarActivation=typename Policy::Activation;
    using HierarchyToolbarDirection=typename Policy::Direction;using HierarchyToolbarError=typename Policy::Error;using HierarchyToolbarErrorCode=typename Policy::ErrorCode;
    template<class T>using HierarchyToolbarResult=typename Policy::template Result<T>;
public:
    BasicHierarchyToolbar() : snapshot_(std::make_shared<const HierarchyToolbarSnapshot>()) {}
    [[nodiscard]] HierarchyToolbarResult<std::shared_ptr<const HierarchyToolbarSnapshot>> snapshot() const {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyToolbarErrorCode::wrong_thread, "Toolbar requires its owner thread");
        return snapshot_;
    }
    [[nodiscard]] HierarchyToolbarResult<bool> set_items(std::vector<HierarchyToolbarItem> items) {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyToolbarErrorCode::wrong_thread, "Toolbar requires its owner thread");
        if (items.size() > 256) return error(HierarchyToolbarErrorCode::invalid, "Toolbar exceeds its item limit");
        auto next = std::make_shared<HierarchyToolbarSnapshot>(); next->items = std::move(items); next->ids.reserve(next->items.size());
        std::map<ToolbarButtonId, std::size_t> keys; std::size_t bytes = 0;
        const auto append = [&](std::size_t count) { if (count > 1024 * 1024 - bytes) return false; bytes += count; return true; };
        for (std::size_t i = 0; i < next->items.size(); ++i) {
            const auto& item = next->items[i];
            if (item.icon_name.size() > 256 || !Policy::valid_text(item.icon_name) || !Policy::valid_text(item.text) ||
                !Policy::valid_text(item.accessibility_name) || !append(item.icon_name.size()) || !append(item.text.size()) ||
                !append(item.accessibility_name.size())) return error(HierarchyToolbarErrorCode::invalid, "Invalid or oversized toolbar item text");
            auto id = item.id.value_or(ToolbarButtonId{std::int64_t(i)});
            if (const auto* text = std::get_if<std::string>(&id); text && (text->empty() || text->size() > 1024 || !Policy::valid_text(*text) || !append(text->size())))
                return error(HierarchyToolbarErrorCode::invalid, "Invalid toolbar item ID");
            if (!keys.emplace(id, i).second) return error(HierarchyToolbarErrorCode::invalid, "Duplicate toolbar item ID");
            next->ids.push_back(std::move(id));
        }
        const auto eligible = [&](std::size_t i) { return next->items[i].enabled && next->items[i].visible; };
        if (snapshot_->active) if (auto found = keys.find(snapshot_->active->id); found != keys.end() && eligible(found->second))
            next->active = HierarchyToolbarSelection{found->first, found->second};
        if (!next->active) for (std::size_t i = 0; i < next->items.size(); ++i)
            if (eligible(i) && next->items[i].selected) { next->active = HierarchyToolbarSelection{next->ids[i], i}; break; }
        if (!next->active) for (std::size_t i = 0; i < next->items.size(); ++i)
            if (eligible(i)) { next->active = HierarchyToolbarSelection{next->ids[i], i}; break; }
        if (snapshot_->items == next->items && snapshot_->active == next->active) return false;
        if (snapshot_->revision == (std::numeric_limits<std::uint64_t>::max)()) return error(HierarchyToolbarErrorCode::exhausted, "Toolbar revision exhausted");
        next->revision = snapshot_->revision + 1; indices_ = std::move(keys);snapshot_ = std::move(next); return true;
    }
    [[nodiscard]] HierarchyToolbarResult<bool> set_active(const ToolbarButtonId& id) {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyToolbarErrorCode::wrong_thread, "Toolbar requires its owner thread");
        if (const auto found=indices_.find(id);found!=indices_.end()) {const auto i=found->second;
            if (!snapshot_->items[i].enabled || !snapshot_->items[i].visible) return error(HierarchyToolbarErrorCode::invalid, "Toolbar item is not active");
            if (snapshot_->active && snapshot_->active->id == id) return false;
            if (snapshot_->revision == (std::numeric_limits<std::uint64_t>::max)()) return error(HierarchyToolbarErrorCode::exhausted, "Toolbar revision exhausted");
            auto next = std::make_shared<HierarchyToolbarSnapshot>(*snapshot_);
            next->active = HierarchyToolbarSelection{id, i}; ++next->revision; snapshot_ = std::move(next); return true;
        }
        return error(HierarchyToolbarErrorCode::not_found, "Toolbar item was not found");
    }
    [[nodiscard]] HierarchyToolbarResult<HierarchyToolbarActivation> activate(const ToolbarButtonId& id) {
        auto changed = set_active(id); if (!changed) return std::unexpected(changed.error());
        const auto& selection = *snapshot_->active;
        return HierarchyToolbarActivation{selection, *changed, snapshot_->items[selection.index].command};
    }
    [[nodiscard]] HierarchyToolbarResult<bool> navigate(HierarchyToolbarDirection direction) {
        if (owner_ != std::this_thread::get_id()) return error(HierarchyToolbarErrorCode::wrong_thread, "Toolbar requires its owner thread");
        if (unsigned(direction) > unsigned(HierarchyToolbarDirection::last)) return error(HierarchyToolbarErrorCode::invalid, "Invalid toolbar direction");
        const auto count = std::ptrdiff_t(snapshot_->items.size()); if (!count) return false;
        const bool backwards = direction == HierarchyToolbarDirection::previous || direction == HierarchyToolbarDirection::last;
        auto position = direction == HierarchyToolbarDirection::first ? std::ptrdiff_t(-1) :
            direction == HierarchyToolbarDirection::last ? count : snapshot_->active ? std::ptrdiff_t(snapshot_->active->index) : backwards ? count : std::ptrdiff_t(-1);
        const auto step = backwards ? -1 : 1;
        for (position += step; position >= 0 && position < count; position += step) {
            const auto i = std::size_t(position); if (snapshot_->items[i].enabled && snapshot_->items[i].visible) return set_active(snapshot_->ids[i]);
        }
        return false;
    }
private:
    [[nodiscard]] static std::unexpected<HierarchyToolbarError> error(HierarchyToolbarErrorCode code, std::string message) {
        return std::unexpected(HierarchyToolbarError{code, std::move(message)});
    }
    const std::thread::id owner_ = std::this_thread::get_id();
    std::shared_ptr<const HierarchyToolbarSnapshot> snapshot_;
    std::map<ToolbarButtonId,std::size_t> indices_;
};

} // namespace ii::gui
