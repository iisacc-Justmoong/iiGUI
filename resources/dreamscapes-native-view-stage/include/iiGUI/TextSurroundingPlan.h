#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>

namespace ii::gui::text_surrounding_detail {

struct Range final { std::size_t first=0,last=0; };
struct Ranges final {
    std::array<Range,4> values{};
    std::size_t count=0;
    void append(std::size_t first,std::size_t last) noexcept {
        if(first==last)return;
        if(count&&values[count-1].last==first){values[count-1].last=last;return;}
        values[count++]={first,last};
    }
    [[nodiscard]] std::size_t map(std::size_t point) const noexcept {
        std::size_t removed=0;
        for(std::size_t i=0;i<count;++i){
            const auto range=values[i];
            if(point>range.first)removed+=(std::min)(point,range.last)-range.first;
        }
        return point-removed;
    }
};

// The encompassing range contains the selection, whose text is never deleted.
struct Plan final {
    Range before,after;
    [[nodiscard]] static std::optional<Plan> create(std::size_t total,std::size_t first,std::size_t length,
            std::size_t selected_first,std::size_t selected_length) noexcept {
        if(first>total||length>total-first||selected_first>total||selected_length>total-selected_first)
            return std::nullopt;
        const auto last=first+length,selected_last=selected_first+selected_length;
        if(first>selected_first||last<selected_last)return std::nullopt;
        return Plan{{first,selected_first},{selected_last,last}};
    }
    [[nodiscard]] bool empty() const noexcept {return before.first==before.last&&after.first==after.last;}
    [[nodiscard]] Ranges visible() const noexcept {
        Ranges result;result.append(before.first,before.last);result.append(after.first,after.last);return result;
    }
    // The candidate replaces original_length units at marked_first in the base.
    // Deletions within the candidate are deliberately absent from this result.
    [[nodiscard]] Ranges base(std::size_t marked_first,std::size_t marked_length,std::size_t original_length) const noexcept {
        Ranges result;const auto marked_last=marked_first+marked_length;
        for(const auto range:{before,after}){
            if(range.first<marked_first)result.append(range.first,(std::min)(range.last,marked_first));
            if(range.last>marked_last){
                const auto first=(std::max)(range.first,marked_last);
                result.append(marked_first+original_length+(first-marked_last),
                              marked_first+original_length+(range.last-marked_last));
            }
        }
        return result;
    }
};
} // namespace ii::gui::text_surrounding_detail
