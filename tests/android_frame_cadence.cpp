#include "Platform/AndroidFrameCadence.h"

#include <cstdint>
#include <optional>
#include <stdexcept>

using ii::gui::detail::android_frame_time_from_low32;
using ii::gui::detail::android_legacy_frame_time;

static_assert(android_frame_time_from_low32(0xfffffff0u, INT64_C(0x100000020)) == INT64_C(0xfffffff0));
static_assert(android_frame_time_from_low32(0x80000001u, INT64_C(0x180000101)) == INT64_C(0x180000001));
static_assert(android_frame_time_from_low32(0x20u, INT64_C(0x200000020)) == INT64_C(0x200000020));
static_assert(!android_frame_time_from_low32(0xfffffff0u, 100));
static_assert(!android_frame_time_from_low32(0u, -1));
static_assert(android_legacy_frame_time(std::int32_t{-16}, [] { return std::optional<std::int64_t>{INT64_C(0x100000020)}; }) == INT64_C(0xfffffff0));
static_assert(!android_legacy_frame_time(std::int32_t{-16}, [] { return std::optional<std::int64_t>{}; }));
static_assert(android_legacy_frame_time(INT64_C(0x100000123), [] { return std::optional<std::int64_t>{}; }) == INT64_C(0x100000123));

namespace {
struct Observation {
    int legacy_posts = 0;
    int modern_posts = 0;
    int callbacks = 0;
    std::int64_t timestamp = 0;
};

void legacy_callback(long timestamp, void* value) {
    auto& seen = *static_cast<Observation*>(value);
    ++seen.callbacks;
    seen.timestamp = timestamp;
}
void modern_callback(std::int64_t timestamp, void* value) {
    auto& seen = *static_cast<Observation*>(value);
    ++seen.callbacks;
    seen.timestamp = timestamp;
}
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

void AChoreographer_postFrameCallback(AChoreographer*, AChoreographer_frameCallback callback, void* value) {
    ++static_cast<Observation*>(value)->legacy_posts;
    callback(1234567L, value);
}
#if __ANDROID_API__ >= 29
void AChoreographer_postFrameCallback64(AChoreographer*, AChoreographer_frameCallback64 callback, void* value) {
    ++static_cast<Observation*>(value)->modern_posts;
    callback(INT64_C(0x100000123), value);
}
#endif

int main() {
    volatile std::int32_t frame_source = -16;
    volatile std::int64_t clock_source = INT64_C(0x100000020);
    const auto frame = static_cast<std::int32_t>(frame_source);
    const auto clock = static_cast<std::int64_t>(clock_source);
    const auto restored = android_legacy_frame_time(frame, [clock] { return std::optional<std::int64_t>{clock}; });
    check(restored && *restored == INT64_C(0xfffffff0), "A wrapped 32-bit frame time was not restored");
    check(!android_legacy_frame_time(frame, [] { return std::optional<std::int64_t>{}; }), "A failed monotonic clock was accepted");
    check(android_legacy_frame_time(INT64_C(0x100000123), [] { return std::optional<std::int64_t>{}; }) == INT64_C(0x100000123),
          "A 64-bit legacy frame time incorrectly required the clock");
    AChoreographer choreographer;
    Observation seen;
    ii::gui::detail::post_android_frame<legacy_callback, modern_callback>(&choreographer, &seen);
#if __ANDROID_API__ >= 29
    check(seen.modern_posts == 1 && seen.legacy_posts == 0, "API 29 did not choose the 64-bit callback");
    check(seen.timestamp == INT64_C(0x100000123), "64-bit frame time was truncated");
#else
    check(seen.legacy_posts == 1 && seen.modern_posts == 0, "API 24 selected an unavailable 64-bit callback");
    check(seen.timestamp == 1234567L, "Legacy callback time or payload changed");
#endif
    check(seen.callbacks == 1, "The frame callback was lost or delivered more than once");
}
