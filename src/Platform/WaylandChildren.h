#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// UI-thread-only, non-owning window generations. A callback may remove or
// replace another child while a snapshot is being visited.
template<class Child>
class WaylandChildren final {
public:
    void add(Child* child) {
        if (!child || generations_.contains(child)) throw std::invalid_argument("Invalid or duplicate Wayland child");
        if (next_ == std::numeric_limits<std::uint64_t>::max()) throw std::length_error("Wayland child generation exhausted");
        const auto generation = next_ + 1;
        auto [it, inserted] = generations_.emplace(child, generation);
        if (!inserted) throw std::invalid_argument("Duplicate Wayland child");
        try { entries_.push_back({child, generation}); }
        catch (...) { generations_.erase(it); throw; }
        next_ = generation;
    }
    void remove(Child* child) noexcept {
        generations_.erase(child);
        std::erase_if(entries_, [child](const Entry& entry) { return entry.child == child; });
    }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] Child* back() const noexcept { return entries_.empty() ? nullptr : entries_.back().child; }
    template<class F> void for_each(F&& callback) const {
        const auto snapshot = entries_;
        for (const auto entry : snapshot) {
            auto found = generations_.find(entry.child);
            if (found != generations_.end() && found->second == entry.generation)
                callback(*entry.child);
        }
    }
    template<class F> [[nodiscard]] bool any(F&& predicate) const {
        return std::ranges::any_of(entries_, [&](const Entry& entry) { return predicate(*entry.child); });
    }
private:
    struct Entry final { Child* child; std::uint64_t generation; };
    std::vector<Entry> entries_;
    std::unordered_map<Child*, std::uint64_t> generations_;
    std::uint64_t next_ = 0;
};

} // namespace ii::gui::detail
