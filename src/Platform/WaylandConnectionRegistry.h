#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ii::gui::detail {

// Kept per UI thread; it does not own any native connection. The last
// entry is the most recently focused surviving connection.
template<class Connection>
class WaylandConnectionRegistry final {
public:
    void add(Connection* connection) {
        if (!connection || std::ranges::find(connections_, connection) != connections_.end())
            throw std::invalid_argument("Invalid or duplicate Wayland connection");
        // An unfocused new window must not outrank earlier focus history.
        connections_.insert(connections_.begin(), connection);
        if (!current_) current_ = connection;
    }
    [[nodiscard]] bool select(Connection* connection) noexcept {
        const auto found = std::ranges::find(connections_, connection);
        if (found == connections_.end()) return false;
        std::rotate(found, found + 1, connections_.end());
        current_ = connection;
        return true;
    }
    void remove(Connection* connection) noexcept {
        std::erase(connections_, connection);
        if (current_ == connection) current_ = connections_.empty() ? nullptr : connections_.back();
    }
    [[nodiscard]] Connection* current() const noexcept { return current_; }
private:
    std::vector<Connection*> connections_;
    Connection* current_ = nullptr;
};

} // namespace ii::gui::detail
