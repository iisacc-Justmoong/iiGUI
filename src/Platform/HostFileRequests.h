#pragma once

#include <iiSDKCore/Core/Text/Utf8.h>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// Native file-open callbacks share the host owner thread. A rejected batch
// never changes paths that the operating system has already had accepted.
class HostFileRequests final {
public:
    static constexpr std::size_t maximum_paths = 256, maximum_path_bytes = 131072;
    static constexpr std::size_t maximum_bytes = 4 * 1024 * 1024;

    [[nodiscard]] bool admit(std::vector<std::string> paths) {
        if (paths.size() > maximum_paths - paths_.size()) return false;
        std::size_t bytes = 0;
        for (const auto& path : paths) {
            if (path.empty() || path.size() > maximum_path_bytes || path.find('\0') != path.npos ||
                !ii::core::valid_utf8(path) || path.size() > maximum_bytes - bytes) return false;
            bytes += path.size();
        }
        if (bytes > maximum_bytes - bytes_) return false;
        paths_.reserve(paths_.size() + paths.size());
        for (auto& path : paths) paths_.push_back(std::move(path));
        bytes_ += bytes;
        return true;
    }
    [[nodiscard]] std::vector<std::string> take() noexcept {
        bytes_ = 0;
        return std::exchange(paths_, {});
    }
    template<class Delivery, class Wake>
    void deliver(Delivery delivery, Wake&& wake) {
        auto paths = take();
        if (paths.empty()) return;
        delivery(std::move(paths));
        if (!empty()) std::forward<Wake>(wake)();
    }
    [[nodiscard]] bool empty() const noexcept { return paths_.empty(); }
    void clear() noexcept { paths_.clear(); bytes_ = 0; }

private:
    std::vector<std::string> paths_;
    std::size_t bytes_ = 0;
};

} // namespace ii::gui::detail
