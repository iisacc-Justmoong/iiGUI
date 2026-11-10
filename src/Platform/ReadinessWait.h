#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <limits>
#include <poll.h>

namespace ii::gui::detail {
enum class Readiness { ready, timeout, failed };

// Block for readiness or the caller's actual deadline. Interrupted system
// calls resume against the same deadline rather than restarting the timeout.
inline Readiness wait_readable_until(int descriptor, std::chrono::steady_clock::time_point deadline) noexcept {
    if (descriptor < 0) return Readiness::failed;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return Readiness::timeout;
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
        const auto timeout = static_cast<int>(std::min<std::chrono::milliseconds::rep>(
            remaining.count(), (std::numeric_limits<int>::max)()));
        pollfd fd{descriptor, POLLIN, 0};
        const int result = ::poll(&fd, 1, timeout);
        if (result < 0) {
            if (errno == EINTR) continue;
            return Readiness::failed;
        }
        if (result == 0) continue;
        if (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) return Readiness::failed;
        if (fd.revents & POLLIN) return Readiness::ready;
    }
}
} // namespace ii::gui::detail
