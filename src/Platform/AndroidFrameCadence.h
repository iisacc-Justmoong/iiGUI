#pragma once

#include <android/choreographer.h>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace ii::gui::detail {

// The legacy callback truncates its monotonic timestamp on 32-bit Android.
// Pick the most recent time with these low bits; delays of a full wrap are ambiguous.
constexpr std::optional<std::int64_t> android_frame_time_from_low32(
    std::uint32_t low, std::int64_t now) noexcept {
    if (now < 0) return std::nullopt;
    const auto elapsed = static_cast<std::uint32_t>(static_cast<std::uint32_t>(now) - low);
    if (static_cast<std::uint64_t>(elapsed) > static_cast<std::uint64_t>(now)) return std::nullopt;
    return now - elapsed;
}

template<class Frame, class Clock>
constexpr std::optional<std::int64_t> android_legacy_frame_time(Frame frame, Clock clock) {
    static_assert(std::is_integral_v<Frame> && std::is_signed_v<Frame> && sizeof(Frame) >= 4);
    if constexpr (sizeof(Frame) >= sizeof(std::int64_t)) {
        return static_cast<std::int64_t>(frame);
    } else {
        auto now = clock();
        if (!now) return std::nullopt;
        return android_frame_time_from_low32(static_cast<std::uint32_t>(frame), *now);
    }
}

template<auto LegacyCallback, auto ModernCallback>
void post_android_frame(AChoreographer* choreographer, void* context) noexcept {
#if __ANDROID_API__ >= 29
    AChoreographer_postFrameCallback64(choreographer, ModernCallback, context);
#else
    AChoreographer_postFrameCallback(choreographer, LegacyCallback, context);
#endif
}

} // namespace ii::gui::detail
