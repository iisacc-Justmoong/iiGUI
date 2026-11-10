#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace ii::gui::detail {

// Owner-thread association of asynchronous UIKit requests, persistent
// sessions and transient scene instances. Tokens are never reused by a router.
class UIKitSceneRouter final {
public:
    using Token = std::uint64_t;
    enum class Status { connected, reconnected, duplicate, cancelled, disconnected, discarded,
                        failed, ignored, unknown, invalid, conflict };
    struct Event final {
        Status status = Status::unknown;
        Token token = 0;
    };
    static constexpr std::size_t maximum_requests = 4096;
    static constexpr std::size_t maximum_session_bytes = 4096;

    [[nodiscard]] std::optional<Token> reserve() {
        if (!next_ || slots_.size() == maximum_requests) return std::nullopt;
        const Token token = next_;
        slots_.emplace(token, Slot{});
        next_ = token == std::numeric_limits<Token>::max() ? 0 : token + 1;
        return token;
    }

    // A zero token is permitted only for reconnecting an already bound session.
    [[nodiscard]] Event connect(std::string_view session, const void* scene, Token token = 0) {
        if (!valid_session(session) || !scene) return {Status::invalid};
        auto found = slots_.end();
        for (auto it = slots_.begin(); it != slots_.end(); ++it) {
            if (it->second.session == session) { found = it; break; }
        }
        if (found != slots_.end()) {
            if (token && token != found->first) return {Status::conflict};
        } else {
            if (!token || (found = slots_.find(token)) == slots_.end()) return {Status::unknown};
            if (!found->second.session.empty()) return {Status::conflict};
        }
        auto& slot = found->second;
        if (slot.scene == scene) return {Status::duplicate, found->first};
        if (slot.scene) return {Status::conflict};
        for (const auto& [other_token, other] : slots_) {
            if (other_token != found->first && other.scene == scene) return {Status::conflict};
        }
        const bool reconnecting = !slot.session.empty();
        if (!reconnecting) slot.session = std::string(session);
        slot.scene = scene;
        return {slot.cancelled ? Status::cancelled : reconnecting ? Status::reconnected : Status::connected,
                found->first};
    }

    [[nodiscard]] Event disconnect(const void* scene) noexcept {
        if (!scene) return {Status::invalid};
        for (auto& [token, slot] : slots_) {
            if (slot.scene == scene) { slot.scene = nullptr; return {Status::disconnected, token}; }
        }
        return {Status::unknown};
    }

    [[nodiscard]] Event find(const void* scene) const noexcept {
        if (!scene) return {Status::invalid};
        for (const auto& [token, slot] : slots_)
            if (slot.scene == scene) return {slot.cancelled ? Status::cancelled : Status::connected, token};
        return {Status::unknown};
    }

    [[nodiscard]] Event discard(std::string_view session) noexcept {
        if (!valid_session(session)) return {Status::invalid};
        for (auto it = slots_.begin(); it != slots_.end(); ++it) {
            if (it->second.session == session) {
                const auto token = it->first;
                slots_.erase(it);
                return {Status::discarded, token};
            }
        }
        return {Status::unknown};
    }

    // Cancellation retains the reservation until activation fails or the
    // resulting session is discarded, so a late scene cannot reach a new owner.
    [[nodiscard]] bool cancel(Token token) noexcept {
        auto found = slots_.find(token);
        if (found == slots_.end()) return false;
        found->second.cancelled = true;
        return true;
    }

    // A delayed failure cannot retire a session that has already connected.
    [[nodiscard]] Event fail(Token token) noexcept {
        auto found = slots_.find(token);
        if (found == slots_.end()) return {Status::unknown};
        if (!found->second.session.empty()) return {Status::ignored, token};
        slots_.erase(found);
        return {Status::failed, token};
    }

    [[nodiscard]] std::size_t size() const noexcept { return slots_.size(); }
    void clear() noexcept { slots_.clear(); }

private:
    struct Slot final { std::string session; const void* scene = nullptr; bool cancelled = false; };
    [[nodiscard]] static bool valid_session(std::string_view value) noexcept {
        return !value.empty() && value.size() <= maximum_session_bytes && value.find('\0') == value.npos;
    }
    std::map<Token, Slot> slots_;
    Token next_ = 1;
};
} // namespace ii::gui::detail
