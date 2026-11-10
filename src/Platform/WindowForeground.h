#pragma once

#include "HostActivity.h"
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// One application owns this state. Window identities are non-owning and are
// removed before their native hosts retire; a hidden driver is never registered.
class WindowForeground final {
public:
    void set_handler(std::function<void(bool)> handler) { relay_.set_handler(std::move(handler)); }
    [[nodiscard]] bool has_handler() const noexcept { return relay_.has_handler(); }
    void set_hidden(bool hidden) noexcept {
        if (closed_) return;
        hidden_ = hidden;
        observe();
    }
    void open_window(const void* id, bool visible) {
        if (closed_ || !id) return;
        auto found = find(id);
        if (found == windows_.end()) windows_.push_back({id, visible});
        else found->visible = visible;
        seen_window_ = true;
        observe();
    }
    void set_window_visible(const void* id, bool visible) noexcept {
        if (closed_) return;
        auto found = find(id);
        if (found == windows_.end()) return;
        found->visible = visible;
        observe();
    }
    void remove_window(const void* id) noexcept {
        if (closed_) return;
        auto found = find(id);
        if (found == windows_.end()) return;
        windows_.erase(found);
        observe();
    }
    void open() noexcept { relay_.open(); }
    void deliver() { relay_.deliver(); }
    void close() noexcept { closed_ = true; windows_.clear(); relay_.close(); }
private:
    struct Window final { const void* id; bool visible; };
    using Iterator = std::vector<Window>::iterator;
    Iterator find(const void* id) noexcept {
        return std::find_if(windows_.begin(), windows_.end(), [id](const Window& window) { return window.id == id; });
    }
    void observe() noexcept {
        if (!seen_window_) return;
        const bool visible = !hidden_ && std::any_of(windows_.begin(), windows_.end(),
                                                      [](const Window& window) { return window.visible; });
        relay_.observe(visible);
    }
    HostActivity relay_;
    std::vector<Window> windows_;
    bool hidden_ = false, seen_window_ = false, closed_ = false;
};
} // namespace ii::gui::detail
