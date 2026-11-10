#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace ii::gui::detail {

// Owner-thread binding between one logical UIKit session and its current scene.
// A scene object may disappear while its session remains eligible to reconnect.
class UIKitSceneBinding final {
public:
    enum class Connection { invalid, first, reconnected, duplicate, foreign, overlap };

    [[nodiscard]] Connection connect(std::string_view session, const void* scene) {
        if (session.empty() || !scene) return Connection::invalid;
        if (session_.empty()) {
            session_.assign(session);
            scene_ = scene;
            ++revision_;
            return Connection::first;
        }
        if (session_ != session) return Connection::foreign;
        if (scene_ == scene) return Connection::duplicate;
        if (scene_) return Connection::overlap;
        scene_ = scene;
        ++revision_;
        return Connection::reconnected;
    }

    [[nodiscard]] bool accepts(const void* scene) const noexcept { return scene && scene_ == scene; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] bool session_is(std::string_view session) const noexcept {
        return !session_.empty() && session_ == session;
    }
    [[nodiscard]] bool disconnect(const void* scene) noexcept {
        if (!accepts(scene)) return false;
        scene_ = nullptr;
        ++revision_;
        return true;
    }
    [[nodiscard]] bool discard(std::string_view session) noexcept {
        if (!session_is(session)) return false;
        reset();
        return true;
    }
    void reset() noexcept {
        scene_ = nullptr;
        session_.clear();
        ++revision_;
    }

private:
    std::string session_;
    const void* scene_ = nullptr;
    std::uint64_t revision_ = 0;
};
} // namespace ii::gui::detail
