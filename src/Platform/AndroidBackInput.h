#pragma once
#include <android/input.h>
#include <android/keycodes.h>
#include <cstdint>
#include <optional>

namespace ii::gui::detail {
// A Back key belongs to the activity; canceled releases are consumed but do
// not request close. Other input remains available to the widget surface.
inline std::optional<bool> android_back_close(std::int32_t type,std::int32_t code,
                                              std::int32_t action,std::int32_t flags) noexcept {
    if(type!=AINPUT_EVENT_TYPE_KEY||code!=AKEYCODE_BACK)return std::nullopt;
    return action==AKEY_EVENT_ACTION_UP&&(flags&AKEY_EVENT_FLAG_CANCELED)==0;
}
} // namespace ii::gui::detail
