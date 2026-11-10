#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace ii::gui::detail {
// generation, length, anchor, caret, marked start/end and editor flags.
using AndroidImePublication=std::array<std::int64_t,7>;

// Equal geometry/selection does not imply equal text. Region-only revisions
// are already known to the IME and must not trigger updateSelection.
template<class Revision>
[[nodiscard]] inline bool android_ime_publish_required(const std::optional<AndroidImePublication>& previous,
        const AndroidImePublication& next,const std::optional<Revision>& old_revision,
        const std::optional<Revision>& new_revision,bool region_request) noexcept {
    return !previous||*previous!=next||(!region_request&&old_revision!=new_revision);
}

// A region request updates the local marked range but Android already knows
// that range. Publish concurrent text, focus or selection changes normally.
[[nodiscard]] inline bool android_ime_notify_selection(const std::optional<AndroidImePublication>& previous,
        const AndroidImePublication& next,bool region_request) noexcept {
    if(!region_request||!previous)return true;
    return (*previous)[0]!=next[0]||(*previous)[1]!=next[1]||(*previous)[2]!=next[2]||
           (*previous)[3]!=next[3]||(*previous)[6]!=next[6];
}
} // namespace ii::gui::detail
