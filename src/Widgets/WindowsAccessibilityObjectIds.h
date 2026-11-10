#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace ii::gui::detail {
// MSAA custom object IDs are positive and must never identify a later
// generation. Only currently published objects remain in the map.
class Win32AccessibilityObjectIds final {
    std::uint32_t next_=1;
    std::unordered_map<std::string,std::uint32_t> current_;
public:
    [[nodiscard]] std::unordered_map<std::string,std::uint32_t> update(const std::vector<std::string>& identities){
        std::unordered_map<std::string,std::uint32_t> result;result.reserve(identities.size());auto next=next_;
        for(const auto& id:identities){
            if(id.empty()||result.contains(id))continue;
            if(auto found=current_.find(id);found!=current_.end())result.emplace(id,found->second);
            else if(next&&next<=std::uint32_t(std::numeric_limits<std::int32_t>::max())){
                result.emplace(id,next);if(next==std::uint32_t(std::numeric_limits<std::int32_t>::max()))next=0;else ++next;
            }
        }
        auto published=result;current_=std::move(result);next_=next;return published;
    }
};
} // namespace ii::gui::detail
