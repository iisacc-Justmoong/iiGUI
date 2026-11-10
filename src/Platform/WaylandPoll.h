#pragma once

#include "WaylandWaitGroup.h"
#include <cerrno>
#include <limits>
#include <poll.h>
#include <span>
#include <stdexcept>
#include <vector>

namespace ii::gui::detail {

inline bool native_wayland_poll(std::span<WaylandWaitFd> descriptors, int timeout, int wake_fd) {
    if (descriptors.size() > std::numeric_limits<nfds_t>::max())
        throw std::length_error("Too many Wayland wait descriptors");
    std::vector<pollfd> fds;
    fds.reserve(descriptors.size());
    for (const auto& item : descriptors)
        fds.push_back({item.fd, short((item.read ? POLLIN : 0) | (item.write ? POLLOUT : 0)), 0});
    const auto count = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
    if (count < 0) {
        if (errno == EINTR) return false;
        throw std::runtime_error("Wayland poll failed");
    }
    for (std::size_t i = 0; i < fds.size(); ++i) {
        const auto events = fds[i].revents;
        descriptors[i].readable = (events & POLLIN) || (descriptors[i].fd == wake_fd && (events & POLLHUP));
        descriptors[i].writable = events & POLLOUT;
        descriptors[i].error = (events & (POLLERR | POLLNVAL)) ||
                               (descriptors[i].fd != wake_fd && (events & POLLHUP));
    }
    return true;
}

} // namespace ii::gui::detail
