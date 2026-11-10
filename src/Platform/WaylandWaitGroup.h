#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// The only timer for an outstanding transfer is its actual expiry. Round up
// so that a sub-millisecond remainder does not create an early busy loop.
template<class TimePoint>
int limit_wayland_deadline(int current, TimePoint now, TimePoint deadline) {
    if (deadline <= now) return 0;
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
    const auto limited = static_cast<int>((std::min)(remaining, static_cast<decltype(remaining)>((std::numeric_limits<int>::max)())));
    return current < 0 ? limited : (std::min)(current, limited);
}

struct WaylandWaitFd final {
    int fd = -1;
    bool read = false, write = false;
    bool readable = false, writable = false, error = false;
};

// Implementations own their protocol state and remain alive throughout wait.
// prepare() either succeeds with one read prepared or throws with none prepared;
// read_events() releases that preparation even when it reports a failure.
class WaylandWaitSource {
public:
    virtual ~WaylandWaitSource() = default;
    [[nodiscard]] virtual int descriptor() const = 0;
    virtual void dispatch() = 0;
    virtual void prepare() = 0;
    [[nodiscard]] virtual bool flush() = 0; // true means wait for writable
    [[nodiscard]] virtual int limit_timeout(int current) const = 0;
    virtual void append_writable(std::vector<WaylandWaitFd>&) const = 0;
    // Called only for this source's ready or failed transfer descriptor.
    virtual void writable(int descriptor, bool error) = 0;
    virtual void read_events() = 0;
    virtual void cancel() noexcept = 0;
};

// Poll returns false on interruption and throws on a real polling failure.
// Every prepared read is consumed or cancelled before this function returns.
template<class Poll>
void wait_wayland(std::span<WaylandWaitSource* const> sources, int wake_fd, int timeout, Poll&& poll) {
    if (timeout < -1 || (sources.empty() && wake_fd < 0))
        throw std::invalid_argument("Invalid Wayland wait configuration");
    std::unordered_set<const WaylandWaitSource*> unique_sources;
    std::unordered_set<int> unique_fds;
    if (wake_fd >= 0) unique_fds.insert(wake_fd);
    for (auto* source : sources) {
        if (!source || !unique_sources.insert(source).second || source->descriptor() < 0 ||
            !unique_fds.insert(source->descriptor()).second)
            throw std::invalid_argument("Invalid or duplicate Wayland display");
    }

    std::vector<bool> prepared(sources.size(), false);
    struct ReadGuard final {
        std::span<WaylandWaitSource* const> sources;
        std::vector<bool>& prepared;
        ~ReadGuard() {
            for (std::size_t i = 0; i < sources.size(); ++i)
                if (prepared[i]) sources[i]->cancel();
        }
    } guard{sources, prepared};
    std::vector<WaylandWaitFd> descriptors;
    descriptors.reserve(sources.size() + (wake_fd >= 0 ? 1 : 0));
    if (wake_fd >= 0) descriptors.push_back({.fd=wake_fd, .read=true});
    std::vector<std::size_t> displays;
    std::vector<std::pair<std::size_t, std::size_t>> transfers;
    displays.reserve(sources.size());
    transfers.reserve(sources.size());
    for (std::size_t i = 0; i < sources.size(); ++i) {
        auto& source = *sources[i];
        source.dispatch();
        source.prepare();
        prepared[i] = true;
        const bool blocked = source.flush();
        const int limited = source.limit_timeout(timeout);
        if (limited < -1 || (timeout >= 0 && (limited < 0 || limited > timeout)))
            throw std::logic_error("Invalid Wayland timeout limit");
        timeout = limited;
        displays.push_back(descriptors.size());
        descriptors.push_back({.fd=source.descriptor(), .read=true, .write=blocked});
        const auto before = descriptors.size();
        source.append_writable(descriptors);
        transfers.emplace_back(before, descriptors.size());
        for (std::size_t j = before; j < descriptors.size(); ++j) {
            if (descriptors[j].fd < 0 || !descriptors[j].write ||
                !unique_fds.insert(descriptors[j].fd).second)
                throw std::invalid_argument("Invalid or duplicate Wayland transfer descriptor");
        }
    }

    if (!std::forward<Poll>(poll)(std::span<WaylandWaitFd>(descriptors), timeout)) return;
    if (wake_fd >= 0 && descriptors.front().error)
        throw std::runtime_error("Wayland wake descriptor failed");
    for (const auto index : displays)
        if (descriptors[index].error) throw std::runtime_error("Wayland compositor disconnected");
    for (std::size_t i = 0; i < sources.size(); ++i) {
        prepared[i] = false;
        if (descriptors[displays[i]].readable) sources[i]->read_events();
        else sources[i]->cancel();
    }
    for (std::size_t i = 0; i < sources.size(); ++i)
        if (descriptors[displays[i]].writable) (void)sources[i]->flush();
    for (std::size_t i = 0; i < sources.size(); ++i)
        for (auto j = transfers[i].first; j < transfers[i].second; ++j)
            if (descriptors[j].writable || descriptors[j].error)
                sources[i]->writable(descriptors[j].fd, descriptors[j].error);
    for (auto* source : sources) source->dispatch();
}

} // namespace ii::gui::detail
